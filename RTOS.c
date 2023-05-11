/*
*********************************************************************************************************
*
*                           (c) Copyright 2022, Remigiusz Abramik, Szymon Abramik
*
*                                               EXAMPLE #1
*********************************************************************************************************
*/
 
#include "includes.h"
 
/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/
 
#define		TASK_STK_SIZE                 512       /* Size of each task's stacks (# of WORDs)            */
#define		N_TASKS                        15       /* Number of identical tasks                          */
#define		NUL                     ((void*)0)
#define		QueueLenght                    100
#define		numMbox                          5

// nasza struktura
typedef struct 
{
	INT32U	NumerZadania;
	INT32U	obciazenie;
	INT32U	licznik;
	INT32U	zadaneobciazenie;
	INT32U	delta;
	INT32U	NumerSeryjny;
	INT8U	status;
}ramka;

 
/*
*********************************************************************************************************
*                                               VARIABLES
*********************************************************************************************************
*/
 
// Stosy
OS_STK        TaskStacks[N_TASKS][TASK_STK_SIZE]; // Stosy zadan obciazajacych oraz aktywnych zadañ
OS_STK        TaskStartStk[TASK_STK_SIZE];
OS_STK        ReadStk[TASK_STK_SIZE];
OS_STK        EditStk[TASK_STK_SIZE];
OS_STK        DispStk[TASK_STK_SIZE];
OS_STK        SendStk[TASK_STK_SIZE];
 
char			TabZadan[N_TASKS];
void*			Queue[QueueLenght] = {NUL}; 			// kolejka 
INT32U			global = 1;						// zmienna globalna
OS_EVENT		*PrzekaznikMailBox = NUL;
OS_EVENT		*ekranQueue = NUL;
void*			ekranQ[QueueLenght] = {NUL};			//kolejka dla wysylania struktury do wyswietlenia
OS_EVENT		*TransformacjaQ = NUL;			//kolejka do klawiatury
void*			TransformacjaQueue[QueueLenght] = {NUL};
OS_EVENT		*QueueTask = NUL;				//event dla koleki semafora i mailboxa
OS_EVENT		*SemaforTask = NUL;
OS_EVENT		*MailBoxTask[numMbox] = {NUL};
OS_MEM			*QueueMemory = NUL;				//pamiec dla kolejki
ramka			QueueMemoryTab[QueueLenght];
OS_MEM			*MailboxMemory = NUL;			//pamiec dla mailboxow
INT32U			MailBoxMemoryTab[QueueLenght] = {0};


/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/
 
        void  TaskStart(void *data);
static  void  TaskStartCreateTasks(void);
static  void  TaskStartDispInit(void);
static  void  TaskStartDisp(void);
// taski obs³ugujace
		void Klawiatura(void *pdata);
		void Transformacja(void *pdata);
		void Przekaznik(void* pdata);
		void ekran(void *pdata);
// zadania obciazajace
		void TaskSemafor(void *pdata);
		void TaskMailBox(void *pdata);
		void TaskQueue(void *pdata);
/*$PAGE*/
/*
*********************************************************************************************************
*                                                MAIN
*********************************************************************************************************
*/
 
void  main (void)
{
	INT8U err;
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);													/* Clear the screen                         */
    OSInit();																							/* Initialize uC/OS-II                      */
    PC_DOSSaveReturn();																					/* Save environment to return to DOS        */
    PC_VectSet(uCOS, OSCtxSw);																			/* Install uC/OS-II's context switch vector */
	QueueMemory = OSMemCreate(&QueueMemoryTab[0], QueueLenght, sizeof(ramka), &err);							//utworzenie partycji pamiêci do dynamicznego zarz¹dzania pamiêci¹
	MailboxMemory = OSMemCreate(&MailBoxMemoryTab[0], QueueLenght, sizeof(INT32U), &err);
    OSTaskCreate(TaskStart, NUL, &TaskStartStk[TASK_STK_SIZE - 1], 0);
    OSStart();																							/* Start multitasking                       */
}
 
 
/*
*********************************************************************************************************
*                                              STARTUP TASK
*********************************************************************************************************
*/
void  TaskStart (void *pdata)
{
#if OS_CRITICAL_METHOD == 3                                /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr;
#endif
	INT8U i = 0;
	ramka stoper;
	stoper.NumerZadania = 22;
    pdata = pdata;                                         /* Prevent compiler warning                 */
    TaskStartDispInit();                                   /* Initialize the display                   */
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);                           /* Install uC/OS-II's clock tick ISR        */
    PC_SetTickRate(OS_TICKS_PER_SEC);                      /* Reprogram tick rate                      */
    OS_EXIT_CRITICAL();
    OSStatInit();                                          /* Initialize uC/OS-II's statistics         */
    TaskStartCreateTasks();                                /* Create all the application tasks         */
	// Tworzenie kolejek
	TransformacjaQ = OSQCreate(&TransformacjaQueue[0], QueueLenght);
	PrzekaznikMailBox = OSMboxCreate(NUL);
	ekranQueue = OSQCreate(&ekranQ[0], QueueLenght);
	QueueTask = OSQCreate(&Queue[0], QueueLenght);					//kolejka dla zadania obciazajacego
	for(i = 0; i < numMbox; i++)							//Tworzenie MailBoxów dla zadania obciazajacych
	{
		MailBoxTask[i] = OSMboxCreate(NUL);
	}
	// Tworzenie semafora
	SemaforTask = OSSemCreate(1);
    for (;;) {
        TaskStartDisp();									/*Update the display*/
        OSCtxSwCtr = 0;    									/* Clear context switch counter*/
		OSQPost(ekranQueue, &stoper);						//aktualizacja delty
		OSTimeDly(OS_TICKS_PER_SEC);    					/* Wait 1 second*/
	}
}
 
/*$PAGE*/
/*
*********************************************************************************************************
*                                        INITIALIZE THE DISPLAY
*********************************************************************************************************
*/
 
static  void  TaskStartDispInit (void)
{
    PC_DispStr( 0,  0, "                  SO LAB Remigiusz Abramik & Szymon Abramik                      ", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
    PC_DispStr( 0,  1, "                                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  2, "Zadaj wartosc obciazenia    :                                                    ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  3, "Aktualnie zadane obciazenie :                                                    ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  4, "                                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  5, "Task    Priority    Obciazenie       Wejscia          Delta                      ", DISP_FGND_WHITE + DISP_BGND_CYAN);
    PC_DispStr( 0,  5,"                                                                                  ", DISP_FGND_WHITE + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  6, "Queue1    |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  7, "Queue2    |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  8, "Queue3    |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  9, "Queue4    |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 10, "Queue5    |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 11, "Message1  |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 12, "Message2  |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 13, "Message3  |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 14, "Message4  |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 15, "Message5  |                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 16, "Semaphore1|                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 17, "Semaphore2|                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 18, "Semaphore3|                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 19, "Semaphore4|                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 20, "Semaphore5|                                                                      ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 21, "                                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 22, "#Tasks          :            CPU Usage:       %                                  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 23, "#Task switch/sec:        Czestotliwosc:       Hz                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 24, "                                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
}
 
/*$PAGE*/
/*
*********************************************************************************************************
*                                           UPDATE THE DISPLAY
*********************************************************************************************************
*/
 
static  void  TaskStartDisp (void)
{
    char   s[80];
 
    sprintf(s, "%5d", OSTaskCtr);                                  /* Display #tasks running               */
    PC_DispStr(18, 22, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
 
#if OS_TASK_STAT_EN > 0
    sprintf(s, "%3d", OSCPUUsage);                                 /* Display CPU usage in %               */
    PC_DispStr(40, 22, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#endif
 
    sprintf(s, "%5d", OSCtxSwCtr);                                 /* Display #context switches per second */
    PC_DispStr(18, 23, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
 
    sprintf(s, "V%1d.%02d", OSVersion() / 100, OSVersion() % 100); /* Display uC/OS-II's version number    */
    PC_DispStr(75, 24, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
	sprintf(s, " %3d", OS_TICKS_PER_SEC);
	PC_DispStr(40, 23,s,DISP_FGND_YELLOW + DISP_BGND_BLUE);//wyœwietlenie czêstotliwoœci pracy systemu
	
    switch (_8087) {                                               /* Display whether FPU present          */
        case 0:
             PC_DispStr(71, 22, " NO  FPU ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 1:
             PC_DispStr(71, 22, " 8087 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 2:
             PC_DispStr(71, 22, "80287 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 3:
             PC_DispStr(71, 22, "80387 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
    }
}
 
/*$PAGE*/
/*
*********************************************************************************************************
*                                             CREATE TASKS
*********************************************************************************************************
*/
 
static  void  TaskStartCreateTasks (void)
{
    INT8U  i = 0;
 
	OSTaskCreate(Klawiatura, NUL, &ReadStk[TASK_STK_SIZE - 1], 1);
	OSTaskCreate(Transformacja, NUL, &EditStk[TASK_STK_SIZE - 1], 2);
	OSTaskCreate(ekran, NUL, &DispStk[TASK_STK_SIZE - 1], 3);
	OSTaskCreate(Przekaznik, NUL, &SendStk[TASK_STK_SIZE - 1], 4);
 
	for(i = 0; i < 15; i++)
	{
		TabZadan[i] = i + 5;			//priorytet
	}
 //utworzenie zadan obciazajacych
	for (i = 0; i < 5; i++)
	{
        OSTaskCreate(TaskQueue, &TabZadan[i], &TaskStacks[i][TASK_STK_SIZE - 1], i + 5);
    }
	for (i = 5; i < 10; i++)
	{
        OSTaskCreate(TaskMailBox, &TabZadan[i], &TaskStacks[i][TASK_STK_SIZE - 1], i + 5);
    }
	for (i = 10; i < 15; i++)
	{
        OSTaskCreate(TaskSemafor, &TabZadan[i], &TaskStacks[i][TASK_STK_SIZE - 1], i + 5);
    }
}
 
/*
*********************************************************************************************************
*                                                  TASKS
*********************************************************************************************************
*/

 
void Klawiatura(void *pdata)				// czytanie znaków z klawiatury
{
	INT16S key;
	pdata = pdata;
	for(;;)
	{
		if(PC_GetKey(&key) == TRUE)
		{
			OSQPost(TransformacjaQ, &key);	// wys³anie znaków do kolejki do transformacji
		}
		OSTimeDly(9);
	}
}
 

 
void Transformacja(void *pdata)				//zadanie wyczytujace znaki specjalne i operujace educja tekstu
{
	INT8U err = 0;
	INT8S cursor = 0;
	INT8U i = 0;
	INT16S wpisanaliczba = 0;
	INT16S *wpisanaliczbapomocnicza = NUL;				//zmienna pomocnicza
	char aktualnawartosc[11] = {0};			//tablica tymczasowa
	ramka ramkadane;						//urzywana przez nas struktura
	ramka ramkaekran;						//struktura wysylana do wyswietlenia
	ramkadane.NumerZadania  = 2;			//Ustawiamy nr zadania na jego priorytet dla ulatwienia identyfikacji
	ramkaekran.NumerZadania = 23;			//ustawienie nr zad struktury disp na nr identyfikujacy go jako wyswietlanie wpisanych znakow
	pdata = pdata;
	aktualnawartosc[10] = '\0';				// koniec stringa
	for(;;)
	{
		wpisanaliczbapomocnicza = OSQPend(TransformacjaQ, 0, &err);		//odebranie wiadomosci z kolejki
		if(wpisanaliczbapomocnicza != NUL)
		{
			wpisanaliczba = *wpisanaliczbapomocnicza;
		}
		if(isdigit(wpisanaliczba))										// sprawdzenie czy wpisany znak jest liczba
		{
			if(cursor <10)												// ignorowanie znakow poza rozmiarem bufora
			{ 
				aktualnawartosc[cursor] = wpisanaliczba;
				ramkaekran.zadaneobciazenie = strtoul(aktualnawartosc,NUL,0);
				OSQPost(ekranQueue, &ramkaekran);
				cursor++;
			}
			else														// Upewnia sie ze kursor bedzie maks
			{
				cursor = 10;
			}
		}
		if(wpisanaliczba == 0x08)	//BACKSPACE
		{
			cursor--;
			aktualnawartosc[cursor] = ' ';
 
			if(cursor <= 0){
				aktualnawartosc[0] = ' ';
				cursor = 0;			//zeby kursor nie wyszedl poza tablice
			}
			ramkaekran.zadaneobciazenie = strtoul(aktualnawartosc,NUL,0);
			OSQPost(ekranQueue, &ramkaekran);
		}
		if(wpisanaliczba == 0x7F) //DELETE
		{
			do
			{
				aktualnawartosc[cursor] = ' ';
				cursor--;
			}while(cursor != 0);
			aktualnawartosc[cursor] = ' ';
		}
 
		if(wpisanaliczba == 0x0D)			// ENTER
		{
			ramkadane.obciazenie=strtoul(aktualnawartosc,NUL,0);
			OSMboxPost(PrzekaznikMailBox, &ramkadane); 
			ramkadane.obciazenie=strtoul(aktualnawartosc,NUL,0);
			OSQPost(ekranQueue,&ramkadane);
			cursor = 0;
			for(i = 0; i < 10; i++)
				{
					aktualnawartosc[i] = ' ';
				}
			ramkaekran.zadaneobciazenie = strtoul(aktualnawartosc,NUL,0);
			OSQPost(ekranQueue, &ramkaekran); //wysylanie struktury do wyswietlenia
			if(cursor != 0)
			{
				do
				{
					aktualnawartosc[cursor] = ' ';
					cursor--;
				}while(cursor != 0);
				aktualnawartosc[cursor] = ' ';
			}
 
		}
 
		if(wpisanaliczba == 0x1B)			//ESC
		{
			PC_DOSReturn();
		}
 
		
 
 
	}
}
 
void Przekaznik(void *pdata)//zadanie do zajmujace sie komunikacja z zadaniami obciazajacymi
{
	ramka* daneRecev = NUL;			// struktura odbierajaca
	ramka* daneMail = NUL;			
	ramka* daneSend[5] = { NUL };	// struktura wysylajaca
	ramka* temp;					//zmienna tymczasowa
	INT8U  err = 0;					// zmienna do otrzymania bledu
	INT8U i = 0;					//zmienia do iterowania petli 
	INT8U lokalnynumerseryjny= 1; //numer seryjny 
	INT32U *obciazenieSend = NUL; // wskaznik do wyslania obciazenia 
	char buffer[32];
	pdata = pdata;

	for(i=0;i<5;i++) //nadanie numeru seryjnego
	{	
		daneSend[i]->NumerSeryjny=0;
	}
	for(;;)
	{

		daneRecev = OSMboxPend(PrzekaznikMailBox, 0, &err); //otrzymanie obciazenia
 
		if(err == OS_NO_ERR)
		{	

			OSSemPend(SemaforTask, 0, &err); // odebranie semafora
			global = daneRecev->obciazenie; //zmiana obciazenia
			OSSemPost(SemaforTask); // wyslanie obiazenia do taska semafor
 
			//wyslanie struktury do mailboxow
			for(i = 0; i < 5; i++)
			{	
				temp = OSMboxAccept(MailBoxTask[i]);
				if (temp)											//jezeli utraci dane wypisuje komunikat tylko poczta moze stracic dane 
				{
					sprintf(buffer, "DATA LOST");		
					PC_DispStr(0, 4, buffer, DISP_FGND_YELLOW + DISP_BGND_BLUE);
				}

				obciazenieSend = OSMemGet(MailboxMemory, &err);		//rezerwacja pamieci dla mailboxa
				*obciazenieSend = daneRecev->obciazenie;			//otrzymanie obciazenia
				err = OSMboxPost(MailBoxTask[i], obciazenieSend);	//wyslanie na poczte zadania obciazajacego
				if(err == OS_MBOX_FULL)								//jezeli poczta pelna		
				{
					daneMail->NumerZadania = i + 10;				//nadanie numeru zadania
					daneMail->zadaneobciazenie = err;
					OSQPost(ekranQueue, daneMail);					//wyswietlenie na ekranie
				}
				daneSend[i] = OSMemGet(QueueMemory, &err);			//zarezerwowanie pamieci do wyslania 
				daneSend[i]->NumerZadania = i + 5;					//nadanie numeru zadania do wyslania 
				daneSend[i]->obciazenie = daneRecev->obciazenie;	//nadanie obciazenia do wyslania
				daneSend[i]->NumerSeryjny = lokalnynumerseryjny;	// nadanie numeru seryjnego dla kazdego taska
				err = OSQPost(QueueTask, daneSend[i]);				//wyslanie danych do zadania obciazajacego z kolejka
			}
			lokalnynumerseryjny++;									//iteracja numeru seryjnego dla nastepnego taska 

		}
		
	}
}
 
void ekran(void *pdata)// zadanie zajmujace sie wyswietlaniem
{
	ramka	*daneEkran = NUL;		//struktura odbierajaca
	INT32U	NRZadaniaEkran = 0;		
	INT32U	obciazeniaEkran = 1;
	INT32U	obciazenie = 0;
	INT32U	nowaDelta[15] = { 0 };	//tablica wejsc
	INT32U	wejsciaEkran = 0;		//zmienne operacyjne do obslugi display
	INT8U	i = 0;
	INT8U	err = 0;
	char	charObciazenie[16] = {0};
	char	charNumer[16] = {0};
	char	charZadaneObciazenie[16] = {0};
	char	charDelta[16] = {0};
	char	charLicznik[16] = {0};
	char	charNumerZadania[2] = {0};
	char	charStatus[16] = {0};
	char    buffer[16] = {0};
	pdata = pdata;

	for(;;)
	{
		daneEkran = OSQPend(ekranQueue, 0, &err);		//odebranie rzeczy do wyswietlenia
		//odebranie wartosci
		NRZadaniaEkran = daneEkran->NumerZadania;
		wejsciaEkran = daneEkran->licznik;
		obciazeniaEkran = daneEkran->obciazenie;
		//konwersja w string
		sprintf(charNumerZadania, "%d", NRZadaniaEkran);
		sprintf(charObciazenie, "%10lu", obciazeniaEkran);
		sprintf(charLicznik, "%10lu", wejsciaEkran);
		//status zadania
		if (daneEkran->status == 1)
			sprintf(charStatus, "DONE");
		else
			sprintf(charStatus, "BUSY");


			if(NRZadaniaEkran == 2) //Edit
			{	obciazenie = daneEkran->obciazenie;	//po enterze
				sprintf(charZadaneObciazenie, "%10lu", obciazenie);
				PC_DispStr(30, 3, charZadaneObciazenie, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
			}
			else if(NRZadaniaEkran == 23)
			{
				sprintf(charNumer, "%10lu", daneEkran->zadaneobciazenie);
				PC_DispStr(30, 2, charNumer, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
			}
			else
			{
				if(NRZadaniaEkran>4 && NRZadaniaEkran<20)
				{
					if(daneEkran->zadaneobciazenie == OS_MBOX_FULL)	//jezeli poczta pelna
					{
						PC_DispStr(30, daneEkran->NumerZadania + 1, "FULL", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
					}
					else{
						nowaDelta[NRZadaniaEkran - 5]++; // zwiekszanie delty
						PC_DispStr(13, NRZadaniaEkran + 1, charNumerZadania, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(18, NRZadaniaEkran + 1, charObciazenie, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(33, NRZadaniaEkran + 1, charLicznik, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(75, NRZadaniaEkran + 1, charStatus, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
						PC_DispStr(30, daneEkran->NumerZadania + 1, "    ", DISP_FGND_WHITE + DISP_BGND_LIGHT_GRAY);
						sprintf(buffer, "         ");
						PC_DispStr(0, 4, buffer, DISP_FGND_YELLOW + DISP_BGND_BLUE);
						
						}
 
				}else
				{
					for(i = 0; i < 15; i++) //wyswietlanie delty
						{
							sprintf(charDelta, "%10lu", nowaDelta[i]/2);
							PC_DispStr(47, i + 6, charDelta, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
							nowaDelta[i] = 0;
						}
				}
			} 
	}
}
 
void TaskSemafor(void *pdata)
{
	ramka daneSemafor;
	INT32U i = 0;
	INT32U temp = 0;
	//ustawienie watosci poczatkowych 
	daneSemafor.NumerZadania = *(INT8U *)pdata;
	daneSemafor.obciazenie = 1;
	daneSemafor.licznik = 0;
 
	for(;;)
	{
		daneSemafor.status = 0;	//zmiania stanu na busy
		OSSemAccept(SemaforTask);						//pobranie semafora
		daneSemafor.obciazenie = global;				//nadanie wartosci obciazenia
		OSSemPost(SemaforTask);							//odblokowanie semafora
		OSQPost(ekranQueue, &daneSemafor);				//wyslanie do ekranu 
 
		for(i = 0; i < daneSemafor.obciazenie; i++)		//obciazenie
		{
			temp++;
		}
		daneSemafor.licznik++;		
		daneSemafor.status = 1;		//zmiana stanu na done
		OSQPost(ekranQueue, &daneSemafor);
		OSTimeDly(1);
	}
}
 
void TaskMailBox(void *pdata)
{
	INT32U *daneMailbox;		//wskaznik na dane z mailboxa
	ramka daneMailboxWysylane;
	INT32U i = 0;
	INT32U temp = 0;
	//ustawienie watosci poczatkowych 
	daneMailboxWysylane.NumerZadania = *(INT8U *)pdata;
	daneMailboxWysylane.obciazenie = 1;
	for(;;)
	{	
		daneMailboxWysylane.status = 0; //zmiania stanu na busy							//pobranie danych z mailboxow
		daneMailbox = OSMboxAccept(MailBoxTask[daneMailboxWysylane.NumerZadania - 10]);	//nadanie numeru zadania
			if(daneMailbox != NUL)
			{
				daneMailboxWysylane.obciazenie = *daneMailbox;
				OSMemPut(MailboxMemory, daneMailbox);
			}
		OSQPost(ekranQueue, &daneMailboxWysylane);										//wyslanie danych do wyswietlenia
		for(i = 0;  i < daneMailboxWysylane.obciazenie;  i++)
		{
			temp++;
		}
		daneMailboxWysylane.licznik++;	
		daneMailboxWysylane.status = 1;	//zmiana sanu na done 
		OSQPost(ekranQueue, &daneMailboxWysylane);	// wyslanie do ekranu 
		OSTimeDly(1);
	}
}
 
void TaskQueue(void *pdata)
{
	ramka *daneQueue;
	ramka daneQueueWysylane;
	INT32U i = 0;
	INT32U OstatniNumerSeryjny = 0;
	INT32U temp = 1;
	//ustawienie wartosci poczatkowych 
	daneQueueWysylane.NumerZadania = *(INT8U *)pdata;
	daneQueueWysylane.obciazenie = 1;
	daneQueueWysylane.licznik = 0;
 
	for(;;)
	{
		daneQueueWysylane.status = 0; //zmiania stanu na busy
		for(i = 0; i < QueueLenght; i++){
			daneQueue = OSQAccept(QueueTask);			//pobranie obciazenia z kolejki przez cala dlugosc
			if(daneQueue != NUL)						//jezeli cos tam jest
			{
				if(daneQueue->NumerZadania == daneQueueWysylane.NumerZadania&& OstatniNumerSeryjny + 1 == daneQueue->NumerSeryjny)	//warunek otrzymania poprawnej wartosci aktualny numer zadania i wiekszy niz ostatni numer seryjny
				{	
					daneQueueWysylane.obciazenie = daneQueue->obciazenie; //pobranie obciazenia
					OSMemPut(QueueMemory, daneQueue);	//zarezerwowanie pamieci 
					OstatniNumerSeryjny++;	//iteracja numeru seryjnego
					break;
				}else
				{
					OSQPost(QueueTask, daneQueue); // jezeli nie ta ramka to zwroc do kolejki 
				}
			}else
			{
				break;
			}
		}
		OSQPost(ekranQueue, &daneQueueWysylane); //wyslanie danych do ekranu
 
		for(i = 0; i < daneQueueWysylane.obciazenie; i++) //przeciazenie
		{
			temp++;
 
		}
		daneQueueWysylane.licznik++;	//licznik na ekranie 
		daneQueueWysylane.status = 1;	//zmiana stanu na done
		OSQPost(ekranQueue, &daneQueueWysylane);	//wyslanie ponownie 
		OSTimeDly(1); 
	}
}
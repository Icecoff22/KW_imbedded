#include "includes.h"
#define TASK_STK_SIZE 512
#define MOVE		1
#define REMOVAL		2

#define GORIGHT		0
#define GOLEFT		1
#define LEFTPOS(pos) (pos == 0 ? SEM_MAX - 1 : pos - 1)

#define SEM_MAX		3
#define DIR			2
#define COLORS		2
#define Station		10
#define Person_MAX	4


OS_STK TaskStk[15][TASK_STK_SIZE];// Task Stack Memory 지정
void StartBusDrive(void *pdata);
void BusDriving(void* pdata);
void GetOnAndOff(void* pdata);
void ArriveStation(void* pdata);
void StationClear(void* pdata);
void Bus_Person_View();
void ViewClear();
void StationClearFunc(int i);
void DispInit();
OS_EVENT* msg_q;//메세지큐
#define N_MSG		100
void* msg_array[N_MSG];

int main (void)
{
    OSInit(); // uC/OS-II 초기화
	
	msg_q = OSQCreate(msg_array, (INT16U)N_MSG);
    OSTaskCreate(StartBusDrive, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 5);
	OSTaskCreate(StartBusDrive, (void*)0, &TaskStk[1][TASK_STK_SIZE - 1], 6);
    OSTaskCreate(StationClear, (void*)0, &TaskStk[2][TASK_STK_SIZE - 1], 25);
    OSTaskCreate(BusDriving, (void*)0, &TaskStk[3][TASK_STK_SIZE - 1], 30);
    OSTaskCreate(GetOnAndOff, (void*)0, &TaskStk[4][TASK_STK_SIZE - 1], 35);
	for (INT16U i = 0; i < Station; i++) {
		OSTaskCreate(ArriveStation, (void*)0, &TaskStk[i+5][TASK_STK_SIZE - 1], 10+i);
	}
    
    // 테스크 생성 (적어도 1개 이상)
    OSStart(); // 멀티 테스킹 시작
    return 0;
}
//BUS struct
typedef struct {
	INT8U color;
	INT8U posX;
	INT8U posY;
	INT8U state;
	INT8U cur_person;
}BUS;
//Person Struct
typedef struct {
	INT8U color;
	INT8U posX;
	INT8U posY;
	INT8U state;
}Person;


BUS Businfo[DIR][SEM_MAX];
Person PersonInfo[Station][Person_MAX];
INT8U ENDLOAD[DIR] = { 74, 1 };
INT8U BUSSTOP[Station] = { 11, 23, 37, 49, 62, 58, 45, 33, 21, 9 };//정류장 x좌표
INT8S MovePosition[DIR] = { 1, -1 };//right and left
INT8U Station_PersonCnt[Station] = { 0, };//정류장별
OS_EVENT* SemBusStation[Station];
INT8U PersonColor[6] = { DISP_FGND_RED + DISP_BGND_LIGHT_GRAY,//사람 랜덤 색깔
								DISP_FGND_BLUE + DISP_BGND_LIGHT_GRAY,
								DISP_FGND_GREEN + DISP_BGND_LIGHT_GRAY,
								DISP_FGND_YELLOW + DISP_BGND_LIGHT_GRAY,
								DISP_FGND_WHITE + DISP_BGND_LIGHT_GRAY,
								DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY };
INT8U BusColor[COLORS] = { DISP_FGND_BLUE + DISP_BGND_LIGHT_GRAY,
								DISP_FGND_GREEN + DISP_BGND_LIGHT_GRAY };

INT8U StartPoint[DIR][2] = { 3, 11,  71, 18 };

OS_EVENT* SemBusDir[DIR];
void StartBusDrive(void *pdata)
{
	INT8U delay, color, ERR, dir, posX, posY, position = 0;

	srand(time((unsigned int*)0) + (OSTCBCur->OSTCBPrio * 237 >> 4));

	dir = (OSTCBCur->OSTCBPrio % 2);
	//버스 세마포어 생성
	SemBusDir[dir] = OSSemCreate(SEM_MAX);
	//버스 시작좌표
	posX = StartPoint[dir][0];
	posY = StartPoint[dir][1];

	while (TRUE) {
		OSSemPend(SemBusDir[dir], 0, &ERR);

		//select Bus color according to direction
		if (dir % 2)
			color = BusColor[0];
		else
			color = BusColor[1];

		if (position >= SEM_MAX) {
			position = 0;
		}
		//버스 멤버변수 세팅
		Businfo[dir][position].posX = posX;
		Businfo[dir][position].posY = posY;
		Businfo[dir][position].color = color;
		Businfo[dir][position].cur_person = 0;
		Businfo[dir][position].state = MOVE;
		

		position++;
		OSTimeDly(20);
	}
   
}
void BusDriving(void* pdata)
{

	INT8U msg[40];
	INT8U i, j;
	INT16S key;

	DispInit();//초기 화면

	for (;;) {

		if (PC_GetKey(&key)) {    //esc입력 시 프로그램 종료                         
			if (key == 0x1B) {                            
			    exit(0);                                          
            }
		}
		Bus_Person_View();
		PC_GetDateTime(msg);//시간 출력
		PC_DispStr(60, 2, msg, DISP_FGND_YELLOW + DISP_BGND_BLUE);	
		for(i = 0; i < SEM_MAX; i++){
			if (Businfo[GORIGHT][i].state != REMOVAL) {//좌->우 버스 움직임
				if (Businfo[GORIGHT][i].posX < ENDLOAD[GORIGHT]) {
					if(Businfo[GORIGHT][LEFTPOS(i)].posX!=Businfo[GORIGHT][i].posX+MovePosition[GORIGHT]) {
						Businfo[GORIGHT][i].posX += MovePosition[GORIGHT];
					}
				}
				else {//버스 종점 도달한 경우
					Businfo[GORIGHT][i].state = REMOVAL;
					OSSemPost(SemBusDir[GORIGHT]);
				}
			}
			if (Businfo[GOLEFT][i].state != REMOVAL) {//우->좌 버스 움직임
				if (Businfo[GOLEFT][i].posX > ENDLOAD[GOLEFT]) {
					if (Businfo[GOLEFT][LEFTPOS(i)].posX != Businfo[GOLEFT][i].posX + MovePosition[GOLEFT]) {
						Businfo[GOLEFT][i].posX += MovePosition[GOLEFT];
					}
				}
				else {//버스 종점 도달한 경우
					Businfo[GOLEFT][i].state = REMOVAL;
					OSSemPost(SemBusDir[GOLEFT]);
				}
			}
		}
		OSTimeDly(1);
	}
}

void DangerNumber(INT8U i) {//20석 중 8자리 이상 노란색, 15자리 이상 빨간색
	if (Businfo[GORIGHT][i].cur_person < 8)
		Businfo[GORIGHT][i].color = PersonColor[2];
	else if (Businfo[GORIGHT][i].cur_person >= 8 && Businfo[GORIGHT][i].cur_person < 15)
		Businfo[GORIGHT][i].color = PersonColor[3];
	else if (Businfo[GORIGHT][i].cur_person >= 15)
		Businfo[GORIGHT][i].color = PersonColor[0];
	if (Businfo[GORIGHT][i].cur_person > 20)
		Businfo[GORIGHT][i].cur_person = 20;

	if (Businfo[GOLEFT][i].cur_person < 8)
		Businfo[GOLEFT][i].color = PersonColor[1];
	else if (Businfo[GOLEFT][i].cur_person >= 8 && Businfo[GOLEFT][i].cur_person < 15)
		Businfo[GOLEFT][i].color = PersonColor[3];
	else if (Businfo[GOLEFT][i].cur_person >= 15)
		Businfo[GOLEFT][i].color = PersonColor[0];
	if (Businfo[GOLEFT][i].cur_person > 20)
		Businfo[GOLEFT][i].cur_person = 20;
}

void GetOnAndOff(void* pdata)//승하차 기능 함수, 모든 정류장은 같은 기능을 함
{
	INT8U i, err;
	char msg[100];
	srand(time((unsigned int*)0) + (OSTCBCur->OSTCBPrio * 237 >> 4));
	for (;;) {
		for (i = 0; i < SEM_MAX; i++) {//정류장 별 버스 탑승, 하차 인원수 차이로 구현
			if (Businfo[GORIGHT][i].posX == BUSSTOP[0]) {
				if (Businfo[GORIGHT][i].cur_person != 0) 
					Businfo[GORIGHT][i].cur_person = Businfo[GORIGHT][i].cur_person - (rand() % Businfo[GORIGHT][i].cur_person/2);
				Businfo[GORIGHT][i].cur_person += Station_PersonCnt[0];
				DangerNumber(i);
				Businfo[GORIGHT][i].posX += MovePosition[GORIGHT];//승하차 후 이동

				sprintf(msg, "%d", 1);
				err = OSQPost(msg_q, msg);//메세지큐를 활용하여 버스정류장 정보 보내기
				for (i = 0; i < Station_PersonCnt[0]; i++) {
					OSSemPost(SemBusStation[0]);//버스정류장 세마포어 반환
				}
				Station_PersonCnt[0] = 0;
				memset(PersonInfo[0], 0, Person_MAX * sizeof(Person));
				
			}
			else if (Businfo[GORIGHT][i].posX == BUSSTOP[1]) {//2번째정류장, 각 정류장 동일
				if (Businfo[GORIGHT][i].cur_person != 0) 
					Businfo[GORIGHT][i].cur_person = Businfo[GORIGHT][i].cur_person - (rand() % Businfo[GORIGHT][i].cur_person/2);	
				
				Businfo[GORIGHT][i].cur_person += Station_PersonCnt[1];
				DangerNumber(i);
				Businfo[GORIGHT][i].posX += MovePosition[GORIGHT];
				sprintf(msg, "%d", 2);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[1]; i++) {
					OSSemPost(SemBusStation[1]);
				}
				Station_PersonCnt[1] = 0;
				memset(PersonInfo[1], 0, Person_MAX * sizeof(Person));
			}
			else if (Businfo[GORIGHT][i].posX == BUSSTOP[2]) {//3번째정류장, 각 정류장 동일
				if (Businfo[GORIGHT][i].cur_person != 0) 
					Businfo[GORIGHT][i].cur_person = Businfo[GORIGHT][i].cur_person - (rand() % Businfo[GORIGHT][i].cur_person/2);
				
				Businfo[GORIGHT][i].cur_person += Station_PersonCnt[2];
				DangerNumber(i);
				Businfo[GORIGHT][i].posX += MovePosition[GORIGHT];
				sprintf(msg, "%d", 3);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[2]; i++) {
					OSSemPost(SemBusStation[2]);
				}
				Station_PersonCnt[2] = 0;
				memset(PersonInfo[2], 0, Person_MAX * sizeof(Person));
			}
			else if (Businfo[GORIGHT][i].posX == BUSSTOP[3]) {//4번째정류장, 각 정류장 동일
				if (Businfo[GORIGHT][i].cur_person != 0) 
					Businfo[GORIGHT][i].cur_person = Businfo[GORIGHT][i].cur_person - (rand() % Businfo[GORIGHT][i].cur_person/2);
				
				Businfo[GORIGHT][i].cur_person += Station_PersonCnt[3];
				DangerNumber(i);
				Businfo[GORIGHT][i].posX += MovePosition[GORIGHT];
				sprintf(msg, "%d", 4);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[3]; i++) {
					OSSemPost(SemBusStation[3]);
				}
				Station_PersonCnt[3] = 0;
				memset(PersonInfo[3], 0, Person_MAX * sizeof(Person));
			}
			else if (Businfo[GORIGHT][i].posX == BUSSTOP[4]) {//5번째정류장, 각 정류장 동일
				if (Businfo[GORIGHT][i].cur_person != 0) 
					Businfo[GORIGHT][i].cur_person = Businfo[GORIGHT][i].cur_person - (rand() % Businfo[GORIGHT][i].cur_person/2);
				
				Businfo[GORIGHT][i].cur_person += Station_PersonCnt[4];
				DangerNumber(i);
				Businfo[GORIGHT][i].posX += MovePosition[GORIGHT];
				sprintf(msg, "%d", 5);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[4]; i++) {
					OSSemPost(SemBusStation[4]);
				}
				Station_PersonCnt[4] = 0;
				memset(PersonInfo[4], 0, Person_MAX * sizeof(Person));
			}
		
			//우->좌 방향 정류장
			if (Businfo[GOLEFT][i].posX == BUSSTOP[5]) {//5번정류장, 각 정류장 동일
				if (Businfo[GOLEFT][i].cur_person != 0) 
					Businfo[GOLEFT][i].cur_person = Businfo[GOLEFT][i].cur_person - (rand() % Businfo[GOLEFT][i].cur_person/2);
				
				Businfo[GOLEFT][i].cur_person += Station_PersonCnt[5];
				DangerNumber(i);
				Businfo[GOLEFT][i].posX += MovePosition[GOLEFT];
				sprintf(msg, "%d", 6);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[5]; i++) {
					OSSemPost(SemBusStation[5]);
				}
				Station_PersonCnt[5] = 0;
				memset(PersonInfo[5], 0, Person_MAX * sizeof(Person));
			}
			else if (Businfo[GOLEFT][i].posX == BUSSTOP[6]) {//6번정류장, 각 정류장 동일
				if (Businfo[GOLEFT][i].cur_person != 0) 
					Businfo[GOLEFT][i].cur_person = Businfo[GOLEFT][i].cur_person - (rand() % Businfo[GOLEFT][i].cur_person/2);
				
				Businfo[GOLEFT][i].cur_person += Station_PersonCnt[6];
				DangerNumber(i);
				Businfo[GOLEFT][i].posX += MovePosition[GOLEFT];
				sprintf(msg, "%d", 7);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[6]; i++) {
					OSSemPost(SemBusStation[6]);
				}
				Station_PersonCnt[6] = 0;
				memset(PersonInfo[6], 0, Person_MAX * sizeof(Person));
			}
			else if (Businfo[GOLEFT][i].posX == BUSSTOP[7]) {//7번정류장, 각 정류장 동일
				if (Businfo[GOLEFT][i].cur_person != 0) 
					Businfo[GOLEFT][i].cur_person = Businfo[GOLEFT][i].cur_person - (rand() % Businfo[GOLEFT][i].cur_person/2);
				
				Businfo[GOLEFT][i].cur_person += Station_PersonCnt[7];
				DangerNumber(i);
				Businfo[GOLEFT][i].posX += MovePosition[GOLEFT];
				sprintf(msg, "%d", 8);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[7]; i++) {
					OSSemPost(SemBusStation[7]);
				}
				Station_PersonCnt[7] = 0;
				memset(PersonInfo[7], 0, Person_MAX * sizeof(Person));
			}
			else if (Businfo[GOLEFT][i].posX == BUSSTOP[8]) {//8번정류장, 각 정류장 동일
				if (Businfo[GOLEFT][i].cur_person != 0)
					Businfo[GOLEFT][i].cur_person = Businfo[GOLEFT][i].cur_person - (rand() % Businfo[GOLEFT][i].cur_person/2);
				
				Businfo[GOLEFT][i].cur_person += Station_PersonCnt[8];
				DangerNumber(i);
				Businfo[GOLEFT][i].posX += MovePosition[GOLEFT];
				sprintf(msg, "%d", 9);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[8]; i++) {
					OSSemPost(SemBusStation[8]);
				}
				Station_PersonCnt[8] = 0;
				memset(PersonInfo[8], 0, Person_MAX * sizeof(Person));
			}
			else if (Businfo[GOLEFT][i].posX == BUSSTOP[9]) {//9번정류장, 각 정류장 동일
				if (Businfo[GOLEFT][i].cur_person != 0)
					Businfo[GOLEFT][i].cur_person = Businfo[GOLEFT][i].cur_person - (rand() % Businfo[GOLEFT][i].cur_person/2);
				
				Businfo[GOLEFT][i].cur_person += Station_PersonCnt[9];
				DangerNumber(i);
				Businfo[GOLEFT][i].posX += MovePosition[GOLEFT];
				sprintf(msg, "%d", 10);
				err = OSQPost(msg_q, msg);
				for (i = 0; i < Station_PersonCnt[9]; i++) {
					OSSemPost(SemBusStation[9]);
				}
				Station_PersonCnt[9] = 0;
				memset(PersonInfo[9], 0, Person_MAX * sizeof(Person));
			}
		}
		//OSTimeDly(1);
	}

}

void ArriveStation(void* pdata)//각 정류장에 사람들이 기다리는 task
{
	INT8U delay, color, ERR, station_num, posX, posY, position = 0;

	srand(time((unsigned int*)0) + (OSTCBCur->OSTCBPrio * 237 >> 4));

	station_num = (OSTCBCur->OSTCBPrio % Station);

	SemBusStation[station_num] = OSSemCreate(Person_MAX);//정류장 세마포어 생성

	posX = BUSSTOP[station_num];

	while (TRUE) {
		OSSemPend(SemBusStation[station_num], 0, &ERR);


		//정류장별 사람위치에 대한 좌표
		switch (Station_PersonCnt[station_num]) {
		case 0:
			if (station_num < 5) {
				posY = 9;
				Station_PersonCnt[station_num]++;
				break;
			}
			if(station_num>=5) {
				posY = 20;
				Station_PersonCnt[station_num]++;
				break;
			}
		case 1:
			if (station_num < 5) {
				posY = 8;
				Station_PersonCnt[station_num]++;
				break;
			}
			if (station_num >= 5) {
				posY = 21;
				Station_PersonCnt[station_num]++;
				break;
			}
		case 2:
			if (station_num < 5) {
				posY = 7;
				Station_PersonCnt[station_num]++;
				break;
			}
			if (station_num >= 5) {
				posY = 22;
				Station_PersonCnt[station_num]++;
				break;
			}
		case 3:
			if (station_num < 5) {
				posY = 6;
				Station_PersonCnt[station_num]++;
				break;
			}
			if (station_num >= 5) {
				posY = 23;
				Station_PersonCnt[station_num]++;
				break;
			}
		default:
			break;
		}

		//select person color according to direction
		color = PersonColor[(INT8U)(rand() % 6)];
		
		if (position >= Person_MAX) {
			position = 0;
		}
		//사람 멤버변수 설정
		PersonInfo[station_num][position].posX = posX;
		PersonInfo[station_num][position].posY = posY;
		PersonInfo[station_num][position].color = color;
		position++;
	
		delay = (INT8U)(rand() % 6 + 1);
		OSTimeDly(delay);
	}

}

//버스이동 흔적 지우기
void ViewClear()
{

	PC_DispStr(1, 11, "                                                                              ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(1, 12, "                                                                              ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(1, 17, "                                                                              ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(1, 18, "                                                                              ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);

}

void StationClear(void* pdata)//msg큐쓰기
{//정류장에 있던 인원 승차 후 화면에서 삭제
	INT8U err;
	void* msg;
	char* tmp;
	for (;;) {
		msg = OSQPend(msg_q, 0, &err);
		tmp = (char*)msg;
		if (!(strcmp(tmp, "1"))) {
			StationClearFunc(1);
		}
		else if (!(strcmp(tmp, "2"))) {
			StationClearFunc(2);
		}
		else if (!(strcmp(tmp, "3"))) {
			StationClearFunc(3);
		}
		else if (!(strcmp(tmp, "4"))) {
			StationClearFunc(4);
		}
		else if (!(strcmp(tmp, "5"))) {
			StationClearFunc(5);
		}
		else if (!(strcmp(tmp, "6"))) {
			StationClearFunc(6);
		}
		else if (!(strcmp(tmp, "7"))) {
			StationClearFunc(7);
		}
		else if (!(strcmp(tmp, "8"))) {
			StationClearFunc(8);
		}
		else if (!(strcmp(tmp, "9"))) {
			StationClearFunc(9);
		}
		else if (!(strcmp(tmp, "10"))) {
			StationClearFunc(10);
		}
	}
}
void StationClearFunc(int i) {//정류장에 있던 인원 승차 후 화면에서 삭제 구현 함수
	INT8U j;
	switch (i) {
	case 1:
		for (j = 6;j <= 9;j++) {
			PC_DispStr(11, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 2:
		for (j = 6;j <= 9;j++) {
			PC_DispStr(23, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 3:
		for (j = 6;j <= 9;j++) {
			PC_DispStr(37, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 4:
		for (j = 6;j <= 9;j++) {
			PC_DispStr(49, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 5:
		for (j = 6;j <= 9;j++) {
			PC_DispStr(62, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 6:
		for (j = 20;j <= 23;j++) {
			PC_DispStr(58, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 7:
		for (j = 20;j <= 23;j++) {
			PC_DispStr(45, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 8:
		for (j = 20;j <= 23;j++) {
			PC_DispStr(33, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 9:
		for (j = 20;j <= 23;j++) {
			PC_DispStr(21, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	case 10:
		for (j = 20;j <= 23;j++) {
			PC_DispStr(9, j, "  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
		}
		break;
	default:
		break;
	}
}


//버스, 기다리는 사람 그리기
void Bus_Person_View()
{
	INT8U i, j;
	ViewClear();
	char s[7];
	char a[7];
	for (i = 0; i < DIR; i++) {
		for (j = 0; j < SEM_MAX; j++) {
			
			if (Businfo[i][j].state == MOVE) {
				PC_DispStr(Businfo[i][j].posX, Businfo[i][j].posY, "■■", Businfo[i][j].color);
				
				if (i == 0) {
					sprintf(s, "%d/20", Businfo[i][j].cur_person);
					PC_DispStr(Businfo[i][j].posX, Businfo[i][j].posY + 1, s, Businfo[i][j].color);
				}
				else if (i == 1) {
					sprintf(a, "%d/20", Businfo[i][j].cur_person);
					PC_DispStr(Businfo[i][j].posX, Businfo[i][j].posY - 1, a, Businfo[i][j].color);
				}
			}
		}
	}
	for (i = 0; i < Station; i++) {
		for (j = 0; j < Person_MAX; j++) {
			PC_DispStr(PersonInfo[i][j].posX, PersonInfo[i][j].posY, "●", PersonInfo[i][j].color);
			
		}
	}
}
void  DispInit()//초기화면
{
	PC_DispStr(0, 0, "                                                                                ", DISP_FGND_BLACK);
	PC_DispStr(0, 1, "                                                                                ", DISP_FGND_BLACK);
	PC_DispStr(0, 2, "                   ********************************                             ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 3, "                   **BUS PERSONNEL MANAGE PROGRAM**                             ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 4, "                   ********************************                             ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 5, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 6, "        |    |      |    |        |    |      |    |       |    |               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 7, "        |    |      |    |        |    |      |    |       |    |               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 8, "        |    |      |    |        |    |      |    |       |    |               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 9, "        |    |      |    |        |    |      |    |       |    |               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 10, "        |----|      |----|        |----|      |----|       |----|               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 11, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 12, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 13, "  ============================================================================  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 14, "  *****ㅡㅡㅡ>   ㅡㅡㅡ>   ㅡㅡㅡ>   ㅡㅡㅡ>   ㅡㅡㅡ>   ㅡㅡㅡ>   ㅡㅡㅡ>****  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 15, "  *****<ㅡㅡㅡ   <ㅡㅡㅡ   <ㅡㅡㅡ   <ㅡㅡㅡ   <ㅡㅡㅡ   <ㅡㅡㅡ   <ㅡㅡㅡ****  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 16, "  ============================================================================  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 17, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 18, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 19, "        |----|      |----|      |----|      |----|       |----|                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 20, "        |    |      |    |      |    |      |    |       |    |                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 21, "        |    |      |    |      |    |      |    |       |    |                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 22, "        |    |      |    |      |    |      |    |       |    |                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 23, "        |    |      |    |      |    |      |    |       |    |                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr(0, 24, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
}
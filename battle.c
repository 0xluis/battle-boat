#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "timer.h"
#include "LCD.c"
#include "imagedata.h"

//--------Find GCD function --------------------------------------------------
unsigned long int findGCD(unsigned long int a, unsigned long int b)
{
	unsigned long int c;
	while(1){
		c = a%b;
		if(c==0){return b;}
		a = b;
		b = c;
	}
	return 0;
}
//--------End find GCD function ----------------------------------------------

//--------Task scheduler data structure---------------------------------------
// Struct for Tasks represent a running process in our simple real-time operating system.
typedef struct _task {
	/*Tasks should have members that include: state, period,
		a measurement of elapsed time, and a function pointer.*/
	signed char state; //Task's current state
	unsigned long int period; //Task period
	unsigned long int elapsedTime; //Time elapsed since last task tick
	int (*TickFct)(int); //Task tick function
} task;

//--------End Task scheduler data structure-----------------------------------

//--------Shared Variables----------------------------------------------------

#define MISS  2
#define HIT  1
#define BLANK  0

//menu variables and output to next states
unsigned char mode = 0;
unsigned char aiDif = 1;
unsigned char gameRestart = 0;
unsigned char menuFinished = 0;
unsigned char playerTurn = 1;
unsigned char turnFinished = 0;
unsigned char started = 0;

//--------End Shared Variables------------------------------------------------

//--------User defined FSMs---------------------------------------------------
//Enum of States
enum gameMenuStates { NEG, MenuS, MenuDone };
enum gameSetupStates { NEG2, setUpP1, setUpP2, gameStart };
enum gameStates { NEG3, humanTurn, aiTurn, player1Win, player2Win };


// --------END User defined FSMs-----------------------------------------------

//init boards one for P1 one for P2 
//keeps track of hit, blank, or misses
//NOTE the postions are not absolute to LCD screen
//they are based on the 4x4 quads
unsigned char boardP1 [15] [9];
unsigned char boardP2 [15] [9];

//struct for the boats, keeps track of their positions
//and if they are destoryed
typedef struct _boat
{
	//boat can be placed in 2 ways horiz or vertical
	//to keep things simple we'll just treat it like a rectangle even if its just a line
	unsigned char xCos[5];
	unsigned char yCos[5];
	unsigned char destroyed;
} boat;

//the cursor's position on the 15x9 game board
typedef struct _cur
{
	unsigned char x1;
	unsigned char y1;
} cur;


//draw the "boat" when choosing location
void updateBoatPosition(boat pBoat)
{
	LcdRect(pBoat.xCos[0],pBoat.xCos[4]+1,pBoat.yCos[0], pBoat.yCos[4]+1, PIXEL_XOR);
		
	LcdUpdate();
}

//init the cursor to 0,0 position
cur cursor = {0,0};

//the cursor position is not absolute but on the 15x9 grid
//this converts the positon to absolute LCD pixels and draws cursor
void updatePosition()
{
	unsigned char x1, y1, x2, y2;
	x1 = 5;
	y1 = 2;
	x2 = 0;
	y2 = 0;
	
	for(unsigned char x = 0; x < cursor.x1; x++)
	x1 = x1+5;
	for(unsigned char y = 0; y < cursor.y1; y++)
	y1 = y1+5;
	
	x2 = x1+3;
	y2 = y1+3;
	
	LcdRect(x1,x2+1,y1,y2+1, PIXEL_XOR);
}

//initialize the boats first array is x values
//2nd is y values
//last is the amount of damage
boat boat1 = {{0,1,2,3,4},{0,0,0,0,0},0};
boat boat2 = {{0,1,2,3,4},{0,0,0,0,0},0};

//checks if press was valid ie position is Blank
//if miss draw x to the Player board
//if hit draw o to the Player board
char checkPress()
{
	unsigned char x1, y1;
	x1 = 5;
	y1 = 2;
	
	for(unsigned char x = 0; x < cursor.x1; x++)
	x1 = x1+5;
	for(unsigned char y = 0; y < cursor.y1; y++)
	y1 = y1+5;
	
	if(playerTurn == 1)
	{
		if(boardP1[cursor.x1][cursor.y1] == BLANK)
		{
			
			if(cursor.y1 == boat2.yCos[0])
			{
				for(unsigned i=0; i<5;i++)
				{
					if(boat2.xCos[i] == cursor.x1)
					{
						boardP1[cursor.x1][cursor.y1] = HIT;
						drawO(x1,y1,boardCanvasP1);
						boat2.destroyed++;
						return 1;
					}
				}
			}
			drawX(x1,y1,boardCanvasP1);
			boardP1[cursor.x1][cursor.y1] = MISS;
			return 1;
		}
	}
	else if(playerTurn == 2)
	{
		if(boardP2[cursor.x1][cursor.y1] == BLANK)
		{
			if(cursor.y1 == boat1.yCos[0])
			{
				for(unsigned i=0; i<5;i++)
				{
					if(boat1.xCos[i] == cursor.x1)
					{
						boardP2[cursor.x1][cursor.y1] = HIT;
						drawO(x1,y1,boardCanvasP2);
						boat1.destroyed++;
						return 1;
					}
				}
			}
			
			drawX(x1,y1,boardCanvasP2);
			boardP2[cursor.x1][cursor.y1] = MISS;
			return 1;
		}
	}
	
	return 0;
}

int gameMenuTick (int state)
{
	//transitions
	switch(state)
	{
		case NEG:
		state = MenuS;
		break;
		case MenuS:
			if(!(~PINC & 0x01))
			{
				state = MenuS;
			}
			else if(mode != 0 && ~PINC & 0x01)
			{
				state = MenuDone;
			}
		break;
		case MenuDone:
			if(gameRestart)
			{
				state = MenuS;
			}
			else
			{
				state = MenuDone;
			}
		break;
		default:
			state = MenuS;
		break;
	}

	//actions
	switch(state)
	{
		case MenuS:
			if(gameRestart)
			{
				mode = 0;
				aiDif = 1;
				started=0;
				menuFinished = 0;
				boat1.destroyed = 0;
				boat2.destroyed = 0;
				memset(boardP1, 0, sizeof(boardP1[0][0])*15*9);
				memset(boardP2, 0, sizeof(boardP2[0][0])*15*9);
				resetBoard(boardCanvasP1);
				resetBoard(boardCanvasP2);
				playerTurn = 1;
				
				cursor.x1=0;
				cursor.y1=0;
			}
			else if(mode < 2  && ~PINC & 0x04)
			{
				mode++;
			}
			else if(mode > 1 && ~PINC & 0x02)
			{
				mode--;
			}
			
		if(mode != 2)
		{
			LcdImage(mainMenu1);
		}
		else
			LcdImage(mainMenu2);
		break;
		case MenuDone:
			menuFinished = 1;
		break;
		default:
		break;
	}
	if(!menuFinished)
		LcdUpdate();
	return state;
}


int gameSetupTick (int state)
{
	//state transitions
	switch(state)
	{
		case NEG2:
			if(menuFinished && !(~PINC &0x01))
			{
				state = setUpP1;
			}
		break;
		case setUpP1:
			if(~PINC & 0x01 && mode == 2)
			{
				state = setUpP2;
				
			}else if(~PINC & 0x01 && mode == 1){
					LcdClear();
					LcdImage(start);
					LcdUpdate();
					state = gameStart;
			}
		break;
		case gameStart:
			if(gameRestart)
			{
				state = NEG2;
				
				boat reset = {{0,1,2,3,4},{0,0,0,0,0},0};
				memcpy(boat1.xCos,reset.xCos,5);
				memcpy(boat1.yCos,reset.yCos,5);
				memcpy(boat2.xCos,reset.xCos,5);
				memcpy(boat2.yCos,reset.yCos,5);
				boat1.destroyed=0;
				boat2.destroyed=0;
				gameRestart=0;
			
			}
			else
			{
				state = gameStart;
			}
		break;
		case setUpP2:
			if(~PINC & 0x01)
			{
				LcdClear();
				LcdImage(start);
				LcdUpdate();
				state = gameStart;
			}
			else
			{
				state = setUpP2;
			}
		break;
		default:
			state = NEG2;
		break;
	}
	
	//state action
	switch(state)
	{
		case setUpP1:
		
			LcdClear();
			LcdImage(boardImg);
			LcdUpdate();
			
			if(~PINC & 0x10)
			{
				if(boat1.xCos[0]>0)
				{
					boat1.xCos[0]--;
					boat1.xCos[1]--;
					boat1.xCos[2]--;
					boat1.xCos[3]--;
					boat1.xCos[4]--;
				}
			}
			if(~PINC & 0x08)
			{
				if(boat1.xCos[4]<14)
				{	
					boat1.xCos[0]++;
					boat1.xCos[1]++;
					boat1.xCos[2]++;
					boat1.xCos[3]++;
					boat1.xCos[4]++;
				}
			}
			if(~PINC & 0x04)
			{
				if(boat1.yCos[4] < 8)
				{	
					boat1.yCos[0]++;
					boat1.yCos[1]++;
					boat1.yCos[2]++;
					boat1.yCos[3]++;
					boat1.yCos[4]++;
				}
			}
			if(~PINC & 0x02)
			{
				if(boat1.yCos[0] > 0)
				{
					boat1.yCos[0]--;
					boat1.yCos[1]--;
					boat1.yCos[2]--;
					boat1.yCos[3]--;
					boat1.yCos[4]--;
				}
			}
			updateBoatPosition(boat1);
			
		break;
		
		case setUpP2:
			LcdClear();
			LcdImage(boardImg);
			LcdUpdate();
			if(~PINC & 0x10)
			{
				if(boat2.xCos[0]>0)
				{
					boat2.xCos[0]--;
					boat2.xCos[1]--;
					boat2.xCos[2]--;
					boat2.xCos[3]--;
					boat2.xCos[4]--;
				}
			}
			if(~PINC & 0x08)
			{
				if(boat2.xCos[4]<14)
				{
					boat2.xCos[0]++;
					boat2.xCos[1]++;
					boat2.xCos[2]++;
					boat2.xCos[3]++;
					boat2.xCos[4]++;
				}
			}
			if(~PINC & 0x04)
			{
				if(boat2.yCos[4] < 8)
				{
					boat2.yCos[0]++;
					boat2.yCos[1]++;
					boat2.yCos[2]++;
					boat2.yCos[3]++;
					boat2.yCos[4]++;
				}
			}
			if(~PINC & 0x02)
			{
				if(boat2.yCos[0] > 0)
				{
					boat2.yCos[0]--;
					boat2.yCos[1]--;
					boat2.yCos[2]--;
					boat2.yCos[3]--;
					boat2.yCos[4]--;
				}
			}
			updateBoatPosition(boat2);
		break;
		
		case gameStart:
			started=1;
		break;
		
		default:
		break;
	}
	
	return state;
}

int gameTick (int state)
{
	//transitions
	switch(state)
	{
		case NEG3:
			if(menuFinished && started && !(~PINC & 0x01))
				state = humanTurn;
		break;
		
		case humanTurn:
		if(boat1.destroyed == 5)
		{
			state = player2Win;
			LcdClear();
			LcdImage(trophyImg);
			LcdUpdate();
		}
		else if(boat2.destroyed == 5)
		{
			state = player1Win;
				LcdClear();
				LcdImage(trophyImg);
				LcdUpdate();
		}

		else if(mode == 2)
		{
			state = humanTurn;
		}
		else if(mode == 1)
		{
			if(playerTurn == 1)
				state = humanTurn;
			else
				state = aiTurn;
		}
		break;
		
		case aiTurn:
		if(playerTurn == 1)
			state = humanTurn;
		else
			state = aiTurn;
		break;
		
		case player1Win:
		//print player 1 win screen;
		if(!(~PINC & 0x01))
		{
			state = player1Win;
		}
		else
		{
			gameRestart = 1;
			//started = 0;
			state = NEG3;
		}
		break;
		
		case player2Win:
		//print player 2 win screen
		if(!(~PINC & 0x01))
		{
			state = player2Win;
		}
		else
		{
			gameRestart = 1;
			//started=0;
			state = NEG3;
		}
		break;
		default:
			state = NEG3;
		break;
	}
	
	//actions
	switch(state)
	{
		case humanTurn:
			
			if(~PINC & 0x10)
			{
				if(cursor.x1>0)
				{
					cursor.x1--;
				}
			}
			if(~PINC & 0x08)
			{
				if(cursor.x1<14)
				{
					cursor.x1++;
				}
			}
			if(~PINC & 0x04)
			{
				if(cursor.y1 < 8)
				{
					cursor.y1++;
				}
			}
			if(~PINC & 0x02)
			{
				if(cursor.y1 > 0)
				{
					cursor.y1--;
				}
			}
			
			LcdClear();
			if(playerTurn==2)
			{
				
				LcdImage2(boardCanvasP2);
				updatePosition();
			}
			else
			{
				
				LcdImage2(boardCanvasP1);
				updatePosition();
				//LcdUpdate();
			}
			
			LcdUpdate();
			
			if(~PINC & 0x01 && checkPress())
			{
				turnFinished = 1;
				if(playerTurn == 1)
					playerTurn = 2;
				else
					playerTurn = 1;
					
				LcdClear();
				LcdImage(switchImg);
				LcdUpdate();
				Delay();
				Delay();
				Delay();
			}
			
			
		break;
		
		case aiTurn:
		cursor.x1 = rand() % 14;
		cursor.y1 = rand() % 8;
		if(checkPress())
			playerTurn = 1;
		break;
		
		case player1Win:
			//print the winner screen	
			//done on transition to screen so we dont have
			//to update LCD each time
		break;
		
		case player2Win:
			//print the winner screen	
			//done on transition to screen so we dont have
			//to update LCD each time
		break;
		default:
		break;
	}
	return state;
}


// Implement scheduler code from PES.
int main()
{
	// Set Data Direction Registers
	// Buttons PORTA[0-7], set AVR PORTA to pull down logic
	DDRB = 0xFF;
	PORTB = 0x00;
	DDRC = 0x00;
	PORTC = 0xFF;
	// . . . etc

	// Period for the tasks
	unsigned long int gameMenuTick_calc = 100;
	unsigned long int gameSetupTick_calc = 200;
	unsigned long int gameTick_calc = 100;

	//Calculating GCD
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(gameMenuTick_calc, gameSetupTick_calc);
	tmpGCD = findGCD(tmpGCD, gameTick_calc);

	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;

	//Recalculate GCD periods for scheduler
	unsigned long int gameMenuTick_period = gameMenuTick_calc/GCD;
	unsigned long int gameSetupTick_period = gameSetupTick_calc/GCD;
	unsigned long int gameTick_period = gameTick_calc/GCD;

	//Declare an array of tasks 
	static task task1, task2, task3;
	task *tasks[] = { &task1, &task2, &task3};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	// Task 1
	task1.state = NEG;//Task initial state.
	task1.period = gameMenuTick_period;//Task Period.
	task1.elapsedTime = gameMenuTick_period;//Task current elapsed time.
	task1.TickFct = &gameMenuTick;//Function pointer for the tick.

	// Task 2
	task2.state = NEG2;//Task initial state.
	task2.period = gameSetupTick_period;//Task Period.
	task2.elapsedTime = gameSetupTick_period;//Task current elapsed time.
	task2.TickFct = &gameSetupTick;//Function pointer for the tick.

	// Task 3
	task3.state = NEG3;//Task initial state.
	task3.period = gameTick_period;//Task Period.
	task3.elapsedTime = gameTick_period;//Task current elapsed time.
	task3.TickFct = &gameTick;//Function pointer for the tick.

	LcdInit();
	LcdContrast(70);
	LcdImage(splashImg);
	LcdUpdate();
	//splash screen wait for gamer to press B...will do select line shenaginas later
	while(!(~PINC & 0x01));
	LcdClear();
	LcdUpdate();
	// Set the timer and turn it on
	TimerSet(GCD);
	TimerOn();

	unsigned short i; // Scheduler for-loop iterator
	while(1) {
		// Scheduler code
		for ( i = 0; i < numTasks; i++ ) {
			// Task is ready to tick
			if ( tasks[i]->elapsedTime == tasks[i]->period ) {
				// Setting next state for task
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				// Reset the elapsed time for next tick.
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;
			
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}

	// Error: Program should not exit!
	return 0;
}

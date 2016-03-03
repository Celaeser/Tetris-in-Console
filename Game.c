#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h> 
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")
/*
	整理了代码……（其实没有）
	-然后添加了音效和输入敏感度
	-添加了难度递增效果（随分数1000-5000-20000），修正了落地时最后的移动效果 
	2015.9.29
	By Cea 
*/

/* 全局变量定义 */ 
DWORD g_dwScore;		//游戏分数 
DWORD g_dwTime;			//游戏时间 
COORD g_dwPos[4];		//当前方块坐标 
WORD g_wCurrBlock; 		//当前方块编号 
WORD g_wNextBlock; 		//下一个方块编号
HMODULE g_hRc;			//当前程序资源句柄 

/* 常量定义 */
//代表方块出现时初始位置的COORD，可以在纸上模拟形状
//（P.S.:采取这种方法是因为所有的俄罗斯方块都是4块） 
const COORD BlockPos[7][4] = {
	{ {8, -4}, {8, -3}, {8, -2}, {8, -1} },
	{ {10, -4}, {10, -3}, {10, -2}, {8, -2} },
	{ {8, -4}, {8, -3}, {8, -2}, {10, -2} },
	{ {8, -3}, {10, -3}, {8, -2}, {10, -2} },
	{ {10, -4}, {10, -3}, {8, -3}, {8, -2} },
	{ {8, -4}, {8, -3}, {10, -3}, {10, -2} },
	{ {8, -4}, {8, -3}, {10, -3}, {8, -2} },
};

//不同方块的颜色[此处属于Windows SDK开发部分，可以忽略] 
const WORD BlockColors[7] = {
	FOREGROUND_INTENSITY | FOREGROUND_RED,
	FOREGROUND_INTENSITY | FOREGROUND_BLUE,
	FOREGROUND_INTENSITY | FOREGROUND_GREEN,
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
	FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN,
	FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN,
};

/* 函数前置声明 */
void InitGame(HANDLE);
BOOL SystemBehavior(HANDLE, PKEY_EVENT_RECORD);
void PlayerBehavior(HANDLE, PKEY_EVENT_RECORD);
void OverGame(HANDLE);
void RotateBlock(COORD[], COORD);

int main(int argc, char* argv[])
{
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	KEY_EVENT_RECORD kerRec;
	ZeroMemory(&kerRec, sizeof(kerRec));

	InitGame(hStdout);	
	do{
		PlayerBehavior(hStdin, &kerRec);
	}while (SystemBehavior(hStdout, &kerRec));
	
	OverGame(hStdout);

	return 0;
}

void InitGame(HANDLE hStdout)
{
	CONSOLE_CURSOR_INFO cciInfo;
	COORD dwSize;
	COORD dwPrePos[4];
	SMALL_RECT srSize;
	WORD i = 0;
	DWORD dwFact = 0;

	SetConsoleTitle("俄罗斯方块");
	g_hRc = GetModuleHandle(NULL);

	GetConsoleCursorInfo(hStdout, &cciInfo);
	cciInfo.bVisible = FALSE;
	SetConsoleCursorInfo(hStdout, &cciInfo);

	srSize.Left = srSize.Top = 0;
	srSize.Bottom = 19;
	srSize.Right = 30;
	dwSize.X = 31; dwSize.Y = 20;

	SetConsoleWindowInfo(hStdout, TRUE, &srSize);
	SetConsoleScreenBufferSize(hStdout, dwSize);

	for (i = 0, dwSize.X = 20, dwSize.Y = 0; i < 20; ++i, ++dwSize.Y){
		WriteConsoleOutputCharacter(hStdout, "|", 1, dwSize, &dwFact);
	}

	dwSize.X = 21; dwSize.Y = 0;
	WriteConsoleOutputCharacter(hStdout, "时间", 4, dwSize, &dwFact);
	dwSize.X = 22; dwSize.Y = 1;
	WriteConsoleOutputCharacter(hStdout, " 0:00:00", 8, dwSize, &dwFact);
	dwSize.X = 21; dwSize.Y = 4;
	WriteConsoleOutputCharacter(hStdout, "分数", 4, dwSize, &dwFact);
	dwSize.X = 22; dwSize.Y = 5;
	WriteConsoleOutputCharacter(hStdout, "       0", 8, dwSize, &dwFact);
	dwSize.X = 21; dwSize.Y = 13;
	WriteConsoleOutputCharacter(hStdout, "预告", 4, dwSize, &dwFact);
	dwSize.X = 21; dwSize.Y = 14;
	WriteConsoleOutputCharacter(hStdout, "----------", 10, dwSize, &dwFact);
	for (i = 0, dwSize.Y = 15; i < 4; ++i, ++dwSize.Y){
		dwSize.X = 21;
		WriteConsoleOutputCharacter(hStdout, "|", 1, dwSize, &dwFact);
		dwSize.X = 30;
		WriteConsoleOutputCharacter(hStdout, "|", 1, dwSize, &dwFact);
	}
	dwSize.X = 21; dwSize.Y = 19;
	WriteConsoleOutputCharacter(hStdout, "----------", 10, dwSize, &dwFact);

	srand(time(NULL));
	
	g_wCurrBlock = rand() % 7;
	CopyMemory(g_dwPos, BlockPos[g_wCurrBlock], sizeof(COORD) * 4);

	g_wNextBlock = rand() % 7;
	
	for (i = 0, dwSize.X = 22, dwSize.Y = 15; i < 4; ++i){
		dwPrePos[i].X = dwSize.X + BlockPos[g_wNextBlock][i].X - 6;
		dwPrePos[i].Y = dwSize.Y + BlockPos[g_wNextBlock][i].Y + 4;
		
		WriteConsoleOutputCharacter(hStdout, "囗", 2, dwPrePos[i], &dwFact);
		FillConsoleOutputAttribute(hStdout, BlockColors[g_wNextBlock], 2, dwPrePos[i], &dwFact);
	}

	return ;
}

void PlayerBehavior(HANDLE hStdin, PKEY_EVENT_RECORD lpKey)
{
	INPUT_RECORD irInput = { 0 };
	DWORD dwFact = 0;
	int i = 0;
	
	//由于是单线程，输入阻塞，只能使用Peek和Flush结合的方式输入
	//增加输入灵敏度的唯一方法就是增加一次操作循环内Peek输入的次数 
	ZeroMemory(lpKey, sizeof(KEY_EVENT_RECORD));
	for (i = 0; i < 2; ++i, Sleep(50)){
		PeekConsoleInput(hStdin, &irInput, 1, &dwFact);
		FlushConsoleInputBuffer(hStdin);
		
		if (dwFact == 1 && irInput.EventType == KEY_EVENT && irInput.Event.KeyEvent.bKeyDown == TRUE){
			*lpKey = irInput.Event.KeyEvent;
			break;
		}
	}
	
	//此处if判断不可取消，因为Sleep(0)会直接将当前线程挂起（即一直Sleep） 
	if (i != 5){
		Sleep (100 - 20 * i);
	} 

	return ;
}

BOOL SystemBehavior(HANDLE hStdout, PKEY_EVENT_RECORD lpKey)
{
	static BYTE btOccu[10][20] = { { 0 } };
	static DWORD dwPreTime = 0;
	static COORD dwCenterPos = { 10, -2 };
	static BOOL bSet[3] = { FALSE }; 
	static DWORD dwTimeDiff = 5;
	char szOutput[9] = { 0 };
	BOOL bIfLand = FALSE;
	BOOL bIfOver = FALSE;
	DWORD dwFact = 0;
	COORD dwSize = { 0 };
	COORD dwPrePos[4] = { { 0 } };
	int i = 0, j = 0, k = 0;
	
	g_dwTime += 1;

	//清空活动方块的当前位置的绘图，不执行则会出现连体的俄罗斯方块=A= 
	for (i = 0; i < 4; ++i){
		WriteConsoleOutputCharacter(hStdout, "　", 2, g_dwPos[i], &dwFact);
		FillConsoleOutputAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 2, g_dwPos[i], &dwFact);
	}

	//有下降操作和时间(具体间隔根据难度变化)下降，并判断是否落地
	if (g_dwTime - dwPreTime >= dwTimeDiff){
		dwPreTime = g_dwTime;
		for (i = 0; i < 4; ++i){
			if (g_dwPos[i].Y >= 0 && 
			(btOccu[g_dwPos[i].X / 2][g_dwPos[i].Y + 1] == 1 || g_dwPos[i].Y == 19)){
				bIfLand = TRUE;
				PlaySound("W_LAND", g_hRc, SND_RESOURCE | SND_ASYNC);
				break;
			}
		}
		
		if (!bIfLand){
			for (i = 0; i < 4; ++i){
				++g_dwPos[i].Y;
			}
			++dwCenterPos.Y;
		}
	}
	
	if (!bIfLand && lpKey->bKeyDown == TRUE && lpKey->wVirtualKeyCode == VK_DOWN){
		for (i = 0; i < 4; ++i){
			if (g_dwPos[i].Y >= 0 && 
			(btOccu[g_dwPos[i].X / 2][g_dwPos[i].Y + 1] == 1 || g_dwPos[i].Y == 19)){
				bIfLand = TRUE;
				PlaySound("W_LAND", g_hRc, SND_RESOURCE | SND_ASYNC);
				break;
			}
		}
		
		if (!bIfLand){
			for (i = 0; i < 4; ++i){
				++g_dwPos[i].Y;
			}
			++dwCenterPos.Y;
		}
	}

	//移动或者旋转
	if (lpKey->bKeyDown == TRUE && lpKey->wVirtualKeyCode == VK_LEFT){
		BOOL bMovable = TRUE;
		
		for (i = 0; i < 4; ++i){
			if (g_dwPos[i].X == 0 || btOccu[g_dwPos[i].X / 2 - 1][g_dwPos[i].Y] == 1){
				bMovable = FALSE;
				break;
			}
		}
		
		if (bMovable){
			for (i = 0; i < 4; ++i){
				g_dwPos[i].X -= 2;
			}
			dwCenterPos.X -= 2;
		}
	}
	
	if (lpKey->bKeyDown == TRUE && lpKey->wVirtualKeyCode == VK_RIGHT){
		BOOL bMovable = TRUE;
		
		for (i = 0; i < 4; ++i){
			if (g_dwPos[i].X == 18 || btOccu[g_dwPos[i].X / 2 + 1][g_dwPos[i].Y] == 1){
				bMovable = FALSE;
				break;
			}
		}
		
		if (bMovable){
			for (i = 0; i < 4; ++i){
				g_dwPos[i].X += 2;
			}
			dwCenterPos.X += 2;
		}
	}
	
	if (!bIfLand && lpKey->bKeyDown == TRUE && lpKey->wVirtualKeyCode == VK_UP){
		COORD dwPos[4] = { { 0 } }; 
		BOOL bRotatable = TRUE;
		
		//先进行旋转，再做判断是否越界等，如果可行则将现在的位置码修改成旋转后的，不行则不保留 
		CopyMemory(dwPos, g_dwPos, sizeof(COORD) * 4);
		RotateBlock(dwPos, dwCenterPos);
		
		for (i = 0; i < 4; ++i){
			if (dwPos[i].X < 0 || dwPos[i].X >= 20 || dwPos[i].Y < 0 || dwPos[i].Y >= 20 ||
			btOccu[dwPos[i].X / 2][dwPos[i].Y] == 1){
				bRotatable = FALSE;
				break;
			}
		}
		
		if (bRotatable){
			CopyMemory(g_dwPos, dwPos, sizeof(COORD) * 4);
			PlaySound("W_ROTATE", g_hRc, SND_RESOURCE | SND_ASYNC);
		}
		else{
			PlaySound("W_ROTATE_F", g_hRc, SND_RESOURCE | SND_ASYNC);
		} 
	}

	if (bIfLand){
		//落地占领，检查溢出
		for (i = 0; i < 4; ++i){
			if (g_dwPos[i].Y < 0){
				bIfOver = TRUE;
			}
			else{
				btOccu[g_dwPos[i].X / 2][g_dwPos[i].Y] = 1;
			} 
		}
	
		//如果没有溢出，则判断消行，得分，得到新的方块
		if (!bIfOver){
			BOOL bIfLineFull = FALSE;
			WORD wLineCount = 0;
			
			for (i = 19; i >= 0; --i){
				bIfLineFull = TRUE;
				
				for (j = 0; j < 10; ++j){
					if (btOccu[j][i] == 0){
						bIfLineFull = FALSE;
						break;
					}
				}
				
				if (bIfLineFull){
					for (k = i; k > 0; --k){
						for (j = 0; j < 10; ++j) btOccu[j][k] = btOccu[j][k - 1];
					}
					for (j = 0; j < 10; ++j) btOccu[j][0] = 0;
					++wLineCount;
					++i;
				}
			}

			if (wLineCount != 0){
				PlaySound("W_CLEAR", g_hRc, SND_RESOURCE | SND_ASYNC);
				g_dwScore += (1 << (wLineCount - 1)) * 100;

				if (!bSet[0] && g_dwScore > 1000){
					bSet[0] = TRUE;
					dwTimeDiff = 4;
				}
				if (!bSet[1] && g_dwScore > 5000){
					bSet[1] = TRUE;
					dwTimeDiff = 3;
				}
				if (!bSet[2] && g_dwScore > 20000){
					bSet[2] = TRUE;
					dwTimeDiff = 2;
				}
			}
			
			//得到新方块 
			g_wCurrBlock = g_wNextBlock;
			g_wNextBlock = rand() % 7;
			CopyMemory(g_dwPos, BlockPos[g_wCurrBlock], 4 * sizeof(COORD));
			dwCenterPos.X = 10; dwCenterPos.Y = -2;
		}
	}
	

	//重新绘图
	if (bIfLand){
		COORD dwSize = { 0 }; 
		
		for (i = 0; i < 10; ++i){
			for (j = 0; j < 20; ++j){
				dwSize.X = i * 2; dwSize.Y = j;
				//根据占领情况绘图 
				if (btOccu[i][j] == 1){
					WriteConsoleOutputCharacter(hStdout, "囗", 2, dwSize, &dwFact);
					FillConsoleOutputAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 2, dwSize, &dwFact);
				}
				else{
					WriteConsoleOutputCharacter(hStdout, "　", 2, dwSize, &dwFact);
					FillConsoleOutputAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 2, dwSize, &dwFact);
				}
			}
		}

		for (dwSize.X = 22, dwSize.Y = 15; dwSize.Y < 19; ++dwSize.Y){
			FillConsoleOutputCharacter(hStdout, ' ', 8, dwSize, &dwFact);
		}

		for (i = 0, dwSize.X = 22, dwSize.Y = 15; i < 4; ++i){
			dwPrePos[i].X = dwSize.X + BlockPos[g_wNextBlock][i].X - 6;
			dwPrePos[i].Y = dwSize.Y + BlockPos[g_wNextBlock][i].Y + 4;
			
			WriteConsoleOutputCharacter(hStdout, "囗", 2, dwPrePos[i], &dwFact);
			FillConsoleOutputAttribute(hStdout, BlockColors[g_wNextBlock], 2, dwPrePos[i], &dwFact);
		}

	}
	else{
		for (i = 0; i < 4; ++i){
			WriteConsoleOutputCharacter(hStdout, "囗", 2, g_dwPos[i], &dwFact);
			FillConsoleOutputAttribute(hStdout, BlockColors[g_wCurrBlock], 2, g_dwPos[i], &dwFact);
		}
	}
	
	sprintf(szOutput, "%2d:%02d:%02d", g_dwTime / 36000, (g_dwTime / 600) % 60,
		g_dwTime % 600 / 10);
	dwSize.X = 22; dwSize.Y = 1;
	WriteConsoleOutputCharacter(hStdout, szOutput, 8, dwSize, &dwFact);
	sprintf(szOutput, "%8d", g_dwScore);
	dwSize.X = 22; dwSize.Y = 5;
	WriteConsoleOutputCharacter(hStdout, szOutput, 8, dwSize, &dwFact);
	
	return !bIfOver;
}

void OverGame(HANDLE hStdout)
{
	char szInfo[MAXBYTE] = { 0 };
	int i = 0;
	COORD dwSize = { 0 };
	DWORD dwFact = 0;
	
	for (i = 0, dwSize.X = 0, dwSize.Y = 19; i < 20; ++i, --dwSize.Y){
		WriteConsoleOutputCharacter(hStdout, "囗囗囗囗囗囗囗囗囗囗", 20, dwSize, &dwFact);
		PlaySound("W_GAMEOVER", g_hRc, SND_RESOURCE | SND_ASYNC);
		Sleep(100);
	}
	
	sprintf(szInfo, "游戏结束，耗时:%2d:%02d:%02d，得分:%8d", g_dwTime / 36000, (g_dwTime / 600) % 60,
		g_dwTime % 600 / 10, g_dwScore);
	MessageBox(GetConsoleWindow(), szInfo, "游戏结束", MB_OK);

	return ;	
}

void RotateBlock(COORD* lpPos, COORD dwCenterPos)
{
	short X = 0, Y = 0;
	int i = 0;
	
	for (i = 0; i < 4; ++i){
		lpPos[i].X -= dwCenterPos.X;
		lpPos[i].Y -= dwCenterPos.Y;
	}
	
	for (i = 0; i < 4; ++i){
		X = -(lpPos[i].Y << 1);
		Y = lpPos[i].X >> 1;
		lpPos[i].X = X;
		lpPos[i].Y = Y;
	}
	
	for (i = 0; i < 4; ++i){
		lpPos[i].X += dwCenterPos.X;
		lpPos[i].Y += dwCenterPos.Y;
		lpPos[i].X -= 2;
	}
	
	return ;
}

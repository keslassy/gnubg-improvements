
#include "backgammon.h"

#define MAX_NUMTHREADS 16

typedef enum _TaskType {TT_ANALYSEMOVE, TT_TEST, TT_CLOSE} TaskType;

typedef struct _Task
{
	TaskType type;
	struct _Task *pLinkedTask;
} Task;

typedef struct _AnalyseMoveTask
{
	Task task;
	moverecord *pmr;
	list *plGame;
	statcontext *psc;
	matchstate ms;
} AnalyseMoveTask;

extern void MT_InitThreads();
extern void MT_Close();
void MT_AddTask(Task *pt);
int MT_WaitForTasks();
unsigned int MT_GetNumThreads();
void MT_SetNumThreads(unsigned int num);
int MT_Enabled(void);
int MT_GetThreadID();

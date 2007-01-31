
#include "config.h"

#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "multithread.h"
#include <glib.h>
#include <glib/gthread.h>
#include <pthread.h>

#define MULTITHREADED 1

#ifdef MULTITHREADED
#define TRY_COUTING_PROCEESING_UNITS 0
#ifndef WIN32
#define GLIB_THREADS

int GetLogicalProcssingUnitCount(void) {
        return 2;
}

#else
extern int GetLogicalProcssingUnitCount(void);
#endif
#endif

typedef struct _ThreadData
{
#if MULTITHREADED
#ifdef GLIB_THREADS
	GMutex* queueLock;
	GMutex* condMutex;
	GCond* activity;
	int active;
	GCond* alldone;
	pthread_key_t tlsKey;
#else
	DWORD tlsIndex;
	HANDLE queueLock;
	HANDLE activity;
	HANDLE alldone;
#endif
	int addedTasks;
	int doneTasks;
	int totalTasks;
#endif
	GList *tasks;
	int result;
} ThreadData;

ThreadData td;

unsigned int numThreads = 0;

unsigned int MT_GetNumThreads()
{
#if MULTITHREADED
	return numThreads;
#else
	return 1;
#endif
}

int MT_Enabled(void)
{
#if MULTITHREADED
	return TRUE;
#else
	return FALSE;
#endif
}

static void MT_CreateThreads(void);
static void MT_CloseThreads(void);

void MT_SetNumThreads(unsigned int num)
{
	MT_CloseThreads();
	numThreads = num;
	MT_CreateThreads();
}

int MT_DoTask();
void MT_TaskDone(Task *pt);

#if MULTITHREADED
#ifdef GLIB_THREADS
gpointer MT_WorkerThreadFunction(gpointer id)
{
	pthread_setspecific(td.tlsKey, id);
#else
void MT_WorkerThreadFunction(void *id)
{
	TlsSetValue(td.tlsIndex, id);
#endif
	MT_TaskDone(NULL);	/* Thread created */
	do
	{
#ifdef GLIB_THREADS
		if (!td.active)
		{
			g_mutex_lock(td.condMutex);
			g_cond_wait(td.activity, td.condMutex);
			g_mutex_unlock(td.condMutex);
		}
#else
		if (WaitForSingleObject(td.activity, INFINITE) != WAIT_OBJECT_0)
		{
			return;
		}
#endif
	} while (MT_DoTask());
#ifdef GLIB_THREADS
	return NULL;
#endif
}
#endif

static void MT_CreateThreads(void)
{
#if MULTITHREADED
	unsigned int i;
	td.totalTasks = numThreads;	/* Use to monitor number created */
	for (i = 0; i < numThreads; i++)
	{
#ifdef GLIB_THREADS
		if (!g_thread_create(MT_WorkerThreadFunction, GINT_TO_POINTER(i + 1), FALSE, NULL))
#else
		if (_beginthread(MT_WorkerThreadFunction, 0, (void*)(i + 1)) == 0)
#endif
			printf("Failed to create thread\n");
	}
	if (td.doneTasks != td.totalTasks)
	{
/* Wait for all the threads to be created (timeout after 20 seconds) */
#ifdef GLIB_THREADS
		g_mutex_lock(td.condMutex);
		GTimeVal tv = {0, 0};
		g_get_current_time (&tv);
		g_time_val_add (&tv, 20 * 1000 * 1000);
		if (g_cond_timed_wait(td.alldone, td.condMutex, &tv) == FALSE)
#else
		if (WaitForSingleObject(td.alldone, 20 * 1000) == WAIT_TIMEOUT)
#endif
			printf("Not sure all threads created!\n");
#endif
#ifdef GLIB_THREADS
		g_mutex_unlock(td.condMutex);
#endif
	}
	/* Reset counters */
	td.totalTasks = td.doneTasks = 0;
}

static void MT_CloseThreads(void)
{
#if MULTITHREADED
	unsigned int i;
	for (i = 0; i < numThreads; i++)
	{
		Task *pt = (Task*)malloc(sizeof(Task));
		pt->type = TT_CLOSE;
		pt->pLinkedTask = NULL;
		MT_AddTask(pt);
	}
	MT_WaitForTasks();
#endif
}

extern void MT_InitThreads()
{
#if MULTITHREADED

#ifdef GLIB_THREADS
	g_thread_init(NULL);
	if (!g_thread_supported())
		printf("Glib threads not supported!\n");
#endif

	if (numThreads == 0)
#if TRY_COUTING_PROCEESING_UNITS
		numThreads = GetLogicalProcssingUnitCount();
#else
		numThreads = 1;
#endif
	td.tasks = NULL;
	td.totalTasks = td.addedTasks = td.doneTasks = 0;
#ifdef GLIB_THREADS
	td.queueLock = g_mutex_new();
	td.condMutex = g_mutex_new();
	td.activity = g_cond_new();
	td.alldone = g_cond_new();
	td.active = FALSE;
	if (pthread_key_create(&td.tlsKey, NULL) != 0)
		printf("Thread local store failed\n");
	pthread_setspecific(td.tlsKey, NULL);
#else
	td.activity = CreateEvent(NULL, TRUE, FALSE, NULL);
	td.alldone = CreateEvent(NULL, FALSE, FALSE, NULL);
	td.queueLock = CreateMutex(NULL, FALSE, NULL);
	td.tlsIndex = TlsAlloc();
	if (td.tlsIndex == TLS_OUT_OF_INDEXES)
		printf("Thread local store failed\n");
	TlsSetValue(td.tlsIndex, (void*)1);
#endif

	MT_CreateThreads();
#endif
}

#if MULTITHREADED

#ifdef GLIB_THREADS
#define MT_GetLock(mutex) g_mutex_lock(mutex)
#define MT_ReleaseLock(mutex) g_mutex_unlock(mutex)
#else
#define MT_GetLock(mutex) WaitForSingleObject(mutex, INFINITE)
#define MT_ReleaseLock(mutex) ReleaseMutex(mutex)
#endif

#endif

Task *MT_GetTask(void)
{
	Task *task = NULL;
	if (g_list_length(td.tasks) > 0)
	{
		task = (Task*)g_list_first(td.tasks)->data;
		td.tasks = g_list_delete_link(td.tasks, g_list_first(td.tasks));
#if MULTITHREADED
		if (g_list_length(td.tasks) == 0)
		{
#ifdef GLIB_THREADS
			g_mutex_lock(td.condMutex);
			td.active = FALSE;
			g_mutex_unlock(td.condMutex);
#else
			ResetEvent(td.activity);
#endif
		}
#endif
	}

	return task;
}

void MT_AbortTasks(void)
{
	Task *task;
#if MULTITHREADED
	MT_GetLock(td.queueLock);
#endif

	/* Remove tasks from list */
	while ((task = MT_GetTask()) != NULL)
		MT_TaskDone(task);

#if MULTITHREADED
	MT_ReleaseLock(td.queueLock);
#endif
}

int MT_DoTask()
{
	int alive = TRUE;
	Task *task;
#if MULTITHREADED
	MT_GetLock(td.queueLock);
#endif
	task = MT_GetTask();
#if MULTITHREADED
	MT_ReleaseLock(td.queueLock);
#endif

	if (task)
	{
		switch (task->type)
		{
		case TT_ANALYSEMOVE:
		{
			float doubleError;
			AnalyseMoveTask *amt;
AnalyzeDoubleDecison:
			amt = (AnalyseMoveTask *)task;

			if (AnalyzeMove (amt->pmr, &amt->ms, amt->plGame, amt->psc,
						&esAnalysisChequer,
						&esAnalysisCube, aamfAnalysis,
						afAnalysePlayers, &doubleError ) < 0 )
			{
				MT_AbortTasks();
				td.result = -1;
			}
			if (task->pLinkedTask)
			{	/* Need to analyze take/drop decision in sequence */
				task = task->pLinkedTask;
				goto AnalyzeDoubleDecison;
			}
			break;
		}

		case TT_TEST:
			printf("Test: waiting for a second");
			g_usleep(1000000);
			break;
		case TT_CLOSE:
			alive = FALSE;
			break;
		}
#if MULTITHREADED
		MT_GetLock(td.queueLock);
		MT_TaskDone(task);
		MT_ReleaseLock(td.queueLock);
#endif
		return alive;
	}
	else
		return -1;
}

void MT_TaskDone(Task *pt)
{
#if MULTITHREADED
	td.doneTasks++;
	if (td.doneTasks == td.totalTasks)
	{
#ifdef GLIB_THREADS
		g_cond_signal(td.alldone);
#else
		SetEvent(td.alldone);
#endif
	}
#endif
	if (pt)
	{
		free(pt->pLinkedTask);		
		free(pt);
	}
}

void MT_AddTask(Task *pt)
{
#if MULTITHREADED
	MT_GetLock(td.queueLock);

	td.addedTasks++;
#endif
	td.tasks = g_list_append(td.tasks, pt);
	if (g_list_length(td.tasks) == 1)
	{	/* First task */
		td.result = 0;
#if MULTITHREADED
#ifdef GLIB_THREADS
		g_mutex_lock(td.condMutex);
		td.active = TRUE;
		g_cond_broadcast(td.activity);
		g_mutex_unlock(td.condMutex);
#else
		SetEvent(td.activity);
#endif
#endif
	}
#if MULTITHREADED
	MT_ReleaseLock(td.queueLock);
#endif
}

#if MULTITHREADED
int MT_WaitForTasks()
{
	int doneTasks, count = 0, done = FALSE;

	MT_GetLock(td.queueLock);
	td.totalTasks = td.addedTasks;
	if (td.doneTasks == td.totalTasks)
		done = TRUE;
	MT_ReleaseLock(td.queueLock);
	if (!done)
	{
#ifdef GLIB_THREADS
		g_mutex_lock(td.condMutex);
		for (;;)
		{
			GTimeVal tv = {0, 0};
			g_get_current_time (&tv);
			g_time_val_add (&tv, 250 * 1000);
			if (g_cond_timed_wait(td.alldone, td.condMutex, &tv) == TRUE)
				break;
#else
		while (WaitForSingleObject(td.alldone, 250) == WAIT_TIMEOUT)
		{
#endif
				doneTasks = td.doneTasks - count;

				if (doneTasks > 0)
				{
					ProgressValueAdd( doneTasks );
					count += doneTasks;
				}
				else
				{
					SuspendInput();

					while( gtk_events_pending() )
					gtk_main_iteration();

					ResumeInput();
				}
		}
#ifdef GLIB_THREADS
		g_mutex_unlock(td.condMutex);
#endif
	}
	/* Reset counters */
	td.totalTasks = td.addedTasks = td.doneTasks = 0;

	return td.result;
}
#else
int MT_WaitForTasks()
{
	while(MT_DoTask() != -1)
	{
		ProgressValueAdd( 1 );
	}
	return td.result;
}
#endif

extern void MT_Close()
{
#if MULTITHREADED
	MT_CloseThreads();

#ifdef GLIB_THREADS
	g_mutex_free(td.queueLock);
	g_mutex_free(td.condMutex);
	g_cond_free(td.activity);
	g_cond_free(td.alldone);
	pthread_key_delete(td.tlsKey);
#else
	CloseHandle(td.queueLock);
	CloseHandle(td.activity);
	CloseHandle(td.alldone);
	TlsFree(td.tlsIndex);
#endif

#endif
}

int MT_GetThreadID()
{
#if MULTITHREADED
	int ret;
 #ifdef GLIB_THREADS
	ret = 1+GPOINTER_TO_INT(pthread_getspecific(td.tlsKey));
 #else
	ret = (int)TlsGetValue(td.tlsIndex);
 #endif
	return ret - 1;
#else
	return 0;
#endif
}

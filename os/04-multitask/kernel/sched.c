#include "os.h"

/* defined in entry.S */
extern void switch_to(ctx_t *next);
extern taskCB_t TCBRdy[];
extern taskCB_t *TCBRunning;

/* 
 * 1. 定義 Scheduler 專屬的堆疊與上下文 
 * 這是一個常駐的系統任務，不屬於任何使用者
 */
#define SCHED_STACK_SIZE 1024
uint8_t sched_stack[SCHED_STACK_SIZE]; // 實際的記憶體空間
ctx_t sched_ctx;                       // 用來保存 Scheduler 的暫存器狀態 (SP, RA)

/* 前置宣告，讓 sched_init 看得到 */
void scheduler_loop(void);

void sched_init()
{
	w_mscratch(0);

	/* 
	 * 2. 手動初始化 Scheduler 的 Context 
	 * 讓它看起來像是一個隨時準備好執行的函數
	 */
	// 設定 SP 指向堆疊頂端 (Stack Grows Down)
	sched_ctx.sp = (uint32_t)(sched_stack + SCHED_STACK_SIZE);
	// 設定 RA (Return Address) 指向 scheduler_loop 函數的開頭
	sched_ctx.ra = (uint32_t)scheduler_loop;
}

/*
 * 3. 這是使用者任務呼叫的函數 (原本的 schedule)
 * 現在它不再處理排程邏輯，唯一的任務就是「把 CPU 交給 Scheduler」
 */
void schedule()
{
	// 保存當前任務狀態，並切換到 Scheduler 的 Context
	switch_to(&sched_ctx);
}

/*
 * 4. 核心調度迴圈 (The Scheduler Loop)
 * 這個函數運行在 sched_stack 上，不會佔用使用者任務的 Stack
 */
void scheduler_loop()
{
	while (1) {
		taskCB_t *nextTask;
		ctx_t *next;
		taskCB_t *readyQ = NULL;

		/* 
		 * A. 處理剛切換出來的任務 (TCBRunning)
		 * 當我們回到這裡時，代表有某個任務呼叫了 schedule() 並切換給了我們
		 */
		if (TCBRunning != NULL) {
			taskCB_t *currentTask = TCBRunning;
			
			// 如果當前任務是執行狀態 (代表它是自願讓出的)，把它放回 Ready Queue
			// 這裡簡化了優先級判斷，確保它能被排程
			if (currentTask->state == TASK_RUNNING) {
				currentTask->state = TASK_READY;
				list_insert_before((list_t*)&TCBRdy[currentTask->priority], (list_t*)currentTask);
			}
		}

		/* 
		 * B. 挑選下一個任務 (原本 schedule 的後半段邏輯)
		 */
		// get highest priority queue
		for (int i = 0; i < PRIO_LEVEL; i++) {
			if (!list_isempty((list_t*)&TCBRdy[i])) {
				readyQ = &TCBRdy[i];
				break;
			}
		}

		// 如果沒有任務可跑 (Idle)，這裡簡單處理：繼續迴圈直到有任務
		if (readyQ == NULL) {
			continue;
		}

		// get next task
		nextTask = (taskCB_t*)readyQ->node.next;
		next = &nextTask->ctx;
		list_remove((list_t*)nextTask);

		/* 
		 * C. 切換到下一個使用者任務
		 */
		TCBRunning = nextTask;
		nextTask->state = TASK_RUNNING;

		// 切換過去！
		// 注意：switch_to 會保存當前狀態 (Scheduler 的狀態) 到 sched_ctx (或 Stack)
		// 當 User Task 再次呼叫 schedule() 時，我們會回到這裡的下一行
		switch_to(next);
	}
}
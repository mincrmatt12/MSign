#pragma once

#include "crashlog_storage.h"
#include <esp8266/backtrace.h>
#include <FreeRTOS.h>
#include <task.h>
#include <list.h>
#include <freertos/xtensa_context.h>
#include <sdkconfig.h>
#include <esp8266/eagle_soc.h>

namespace crashlogs {
	namespace {
		static const char * const sdesc[] = {
			"PC",   "PS",   "A0",   "A1",
			"A2",   "A3",   "A4",   "A5",
			"A6",   "A7",   "A8",   "A9",
			"A10",  "A11",  "A12",  "A13",
			"A14",  "A15",  "SAR", "EXCCAUSE"
		};
		static const char * const edesc[] = {
			"IllegalInstruction", "Syscall", "InstructionFetchError", "LoadStoreError",
			"Level1Interrupt", "Alloca", "IntegerDivideByZero", "PCValue",
			"Privileged", "LoadStoreAlignment", "res", "res",
			"InstrPDAddrError", "LoadStorePIFDataError", "InstrPIFAddrError", "LoadStorePIFAddrError",
			"InstTLBMiss", "InstTLBMultiHit", "InstFetchPrivilege", "res",
			"InstrFetchProhibited", "res", "res", "res",
			"LoadStoreTLBMiss", "LoadStoreTLBMultihit", "LoadStorePrivilege", "res",
			"LoadProhibited", "StoreProhibited",
		};
	}

	template<typename Printer>
	void print_exc_frame(const Printer& PANIC, void * frame) {
		XtExcFrame *exc_frame = static_cast<XtExcFrame *>(frame);

		void *i_lr = (void *)exc_frame->a0;
		void *i_pc = (void *)exc_frame->pc;
		void *i_sp = (void *)exc_frame->a1;

		const uint32_t *regs = (const uint32_t *)exc_frame;

		PANIC("Register dump:\n");

		for (int i = 0; i < 20; i += 4) {
			for (int j = 0; j < 4; j++) {
				PANIC("%-8s: 0x%08x  ", sdesc[i + j], regs[i + j + 1]);
			}
			PANIC("\n");
		}
		PANIC("EXCVADDR: 0x%08x\n", *(uint32_t *)&exc_frame->excvaddr);

		void *o_pc, *o_sp;

		PANIC("\nBacktrace: %p:%p ", i_pc, i_sp);
		while(xt_retaddr_callee(i_pc, i_sp, i_lr, &o_pc, &o_sp)) {
			PANIC("%p:%p ", o_pc, o_sp);
			if (i_pc == o_pc && i_sp == o_sp) {
				PANIC("(inf)");
				break;
			}
			i_pc = o_pc;
			i_sp = o_sp;
		}
		PANIC("\n");
	}

	// Various globals that we scoop data out of from freertos
	extern "C" List_t * volatile pxDelayedTaskList;				/*< Points to the delayed task list currently being used. */
	extern "C" List_t * volatile pxOverflowDelayedTaskList;		/*< Points to the delayed task list currently being used to hold tasks that have overflowed the current tick count. */
	extern "C" List_t pxReadyTasksLists[ configMAX_PRIORITIES ];/*< Prioritised ready tasks. */
	extern "C" List_t xSuspendedTaskList;					    /*< Tasks that are currently suspended. */
	extern "C" void * volatile pxCurrentTCB;

	template<typename Handler>
	void for_each_rtos_thread(const Handler& func) {
		auto for_each_in_tasklist = [&](List_t *tasklist, const char * state){
			void *next, *first;
			if (listCURRENT_LIST_LENGTH(tasklist) == 0U)
				return;
			listGET_OWNER_OF_NEXT_ENTRY(first, tasklist);
			do
			{
				listGET_OWNER_OF_NEXT_ENTRY(next, tasklist);
				const StaticTask_t * tcb = static_cast<StaticTask_t *>(next);
				func(pxCurrentTCB == tcb ? "active" : state, (const char *)tcb->ucDummy7, tcb->pxDummy1);
			} while (next != first);
		};

		UBaseType_t ready_prio = configMAX_PRIORITIES;
		do
		{
			ready_prio--;
			for_each_in_tasklist(&pxReadyTasksLists[ready_prio], "ready");
		} while (ready_prio > (UBaseType_t)tskIDLE_PRIORITY);

		for_each_in_tasklist(pxDelayedTaskList, "blocked");
		for_each_in_tasklist(pxOverflowDelayedTaskList, "blocked");
		for_each_in_tasklist(&xSuspendedTaskList, "suspended");
	}

	template<typename InnerIgnored>
	bool write_panic_frame(CrashBuffer<InnerIgnored>& buffer, void *frame, int wdt) {
		// If there's already a log, keep it.
		if (buffer.saved_log())
			return false;

		// Otherwise start a new one
		buffer.start_log();

		XtExcFrame *exc_frame = static_cast<XtExcFrame *>(frame);

		auto PANIC = [&](const char * fmt, auto ...args) {
			auto panic_fmt_buffer = buffer.log_scratch_buffer();
			size_t panic_string_size = 0;

			ets_printf(fmt, args...);
			panic_fmt_buffer = buffer.log_scratch_buffer();
			if (panic_fmt_buffer.second > 0) {
				panic_string_size = snprintf(panic_fmt_buffer.first, panic_fmt_buffer.second, fmt, args...);
				buffer.write_log(panic_fmt_buffer.first, panic_string_size);
			}
		};

		PANIC("Panic handler called.\n");

		if (wdt) {
			PANIC("Task watchdog got triggered.\n");
		}
		else
		{
#ifdef CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK
			if (exc_frame->exccause == 1) {
				uint32_t reason = soc_debug_reason();

				if (reason & XCHAL_DEBUGCAUSE_DBREAK_MASK) {
					char name[configMAX_TASK_NAME_LEN];

					strncpy(name, pcTaskGetTaskName(NULL), configMAX_TASK_NAME_LEN);
					PANIC("Stack canary watchpoint triggered (%s)\n", name);
				}
			} else
#endif
			{
				const char *reason = exc_frame->exccause < 30 ? edesc[exc_frame->exccause] : "unknown";

				PANIC("Unhandled exception (%s).\n", reason);
			}
		}

		for_each_rtos_thread([&](const char * state, const char * name, void * task_frame){
			if (task_frame == frame)
				return;
			
			PANIC("Task dump for %s (%s):\n", name, state);
			print_exc_frame(PANIC, task_frame);
		});

		PANIC("Exception trace:\n");
		print_exc_frame(PANIC, frame);

		return true;
	}
#undef PANIC
}

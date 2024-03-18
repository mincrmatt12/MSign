#pragma once

#include "crashlog_storage.h"
#include <esp8266/backtrace.h>
#include <FreeRTOS.h>
#include <task.h>
#include <freertos/xtensa_context.h>
#include <sdkconfig.h>
#include <esp8266/eagle_soc.h>

namespace crashlogs {
	template<typename InnerIgnored>
	void write_panic_frame(CrashBuffer<InnerIgnored>& buffer, void *frame, int wdt) {
		// If there's already a log, keep it.
		if (buffer.saved_log())
			return;

		// Otherwise start a new one
		buffer.start_log();

		XtExcFrame *exc_frame = static_cast<XtExcFrame *>(frame);
		
		static const char *sdesc[] = {
			"PC",   "PS",   "A0",   "A1",
			"A2",   "A3",   "A4",   "A5",
			"A6",   "A7",   "A8",   "A9",
			"A10",  "A11",  "A12",  "A13",
			"A14",  "A15",  "SAR", "EXCCAUSE"
		};
		static const char *edesc[] = {
			"IllegalInstruction", "Syscall", "InstructionFetchError", "LoadStoreError",
			"Level1Interrupt", "Alloca", "IntegerDivideByZero", "PCValue",
			"Privileged", "LoadStoreAlignment", "res", "res",
			"InstrPDAddrError", "LoadStorePIFDataError", "InstrPIFAddrError", "LoadStorePIFAddrError",
			"InstTLBMiss", "InstTLBMultiHit", "InstFetchPrivilege", "res",
			"InstrFetchProhibited", "res", "res", "res",
			"LoadStoreTLBMiss", "LoadStoreTLBMultihit", "LoadStorePrivilege", "res",
			"LoadProhibited", "StoreProhibited",
		};

		void *i_lr = (void *)exc_frame->a0;
		void *i_pc = (void *)exc_frame->pc;
		void *i_sp = (void *)exc_frame->a1;

		void *o_pc;
		void *o_sp;

		auto panic_fmt_buffer = buffer.log_scratch_buffer();
		size_t panic_string_size = 0;

#define PANIC(fmt, ...) {\
		ets_printf(fmt, ##__VA_ARGS__); \
		panic_fmt_buffer = buffer.log_scratch_buffer(); \
		if (panic_fmt_buffer.second > 0) { \
			panic_string_size = snprintf(panic_fmt_buffer.first, panic_fmt_buffer.second, fmt, ##__VA_ARGS__); \
			if (buffer.write_log(panic_fmt_buffer.first, panic_string_size + 1) == buffer.WriteBufferFull) return; \
		}}

		if (wdt)
			PANIC("Task watchdog got triggered.\n");


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
			const uint32_t *regs = (const uint32_t *)exc_frame;

			PANIC("Guru Meditation Error: Core  0 panic'ed (%s). Exception was unhandled.\n", reason);
			PANIC("Core 0 register dump:\n");

			for (int i = 0; i < 20; i += 4) {
				for (int j = 0; j < 4; j++) {
					PANIC("%-8s: 0x%08x  ", sdesc[i + j], regs[i + j + 1]);
				}
				PANIC("\n");
			}
		}

		PANIC("\nBacktrace: %p:%p ", i_pc, i_sp);
		while(xt_retaddr_callee(i_pc, i_sp, i_lr, &o_pc, &o_sp)) {
			PANIC("%p:%p ", o_pc, o_sp);
			i_pc = o_pc;
			i_sp = o_sp;
		}
		PANIC("\n");
	}
#undef PANIC
}

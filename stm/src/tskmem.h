#ifndef MSN_TSKMEM_H
#define MSN_TSKMEM_H

#include <FreeRTOS.h>
#include <task.h>

namespace tskmem {
	template<size_t Stack>
	struct TaskHolder {
		TaskHandle_t create(
			 TaskFunction_t code_ptr,
			 const char * const name,
			 void * const parameter,
			 UBaseType_t priority
		) {
			return xTaskCreateStatic(
				code_ptr,
				name,
				Stack,
				parameter,
				priority,
				task_stack,
				&task_buffer
			);
		}

		template<typename Object>
		TaskHandle_t create(Object& obj, const char * const name, UBaseType_t priority) {
			return create([](void *arg){((Object *)arg)->run();}, name, &obj, priority);
		}

	private:
		StaticTask_t task_buffer;
		StackType_t  task_stack[Stack]{};
	};

	extern TaskHolder<256> srvc;
	extern TaskHolder<512> screen;
	extern TaskHolder<176> dbgtim;
}

#endif

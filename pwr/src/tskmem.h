#ifndef MSN_TSKMEM_H
#define MSN_TSKMEM_H

#include "manual_box.h"
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
			requires requires(Object& r) {
				r.run();
			}
		TaskHandle_t create(Object& obj, const char * const name, UBaseType_t priority) {
			return create([](void *arg){((Object *)arg)->run();}, name, &obj, priority);
		}

		template<typename Object>
			requires requires(Object& r) {
				r.run();
				Object{};
			}
		TaskHandle_t create(util::manual_box<Object> & obj, const char * const name, UBaseType_t priority) {
			return create([](void *arg){
				auto *obj = static_cast<util::manual_box<Object> *>(arg);
				obj->init();
				(*obj)->run();
			}, name, &obj, priority);
		}

	private:
		StaticTask_t task_buffer;
		StackType_t  task_stack[Stack]{};
	};

	extern TaskHolder<256> regman;
}

#endif

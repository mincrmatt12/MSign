#include <FreeRTOSConfig.h>

// Make debugging work with rtos
const int __attribute__((used)) uxTopUsedPriority = configMAX_PRIORITIES - 1;

void __cxa_pure_virtual() {}

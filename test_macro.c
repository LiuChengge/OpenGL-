#include <stdio.h>
int main() {
#ifdef USE_VULKAN
printf("USE_VULKAN defined, value: %d\n", USE_VULKAN);
#else
printf("USE_VULKAN not defined\n");
#endif
return 0;
}

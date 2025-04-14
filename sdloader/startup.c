#include <soc/hw_init.h>
#include <storage/emmc.h>
#include <storage/sd.h>
#include <utils/types.h>
#include <mem/heap.h>
#include <mem/mc.h>
#include <utils/util.h>
#include <memory_map.h>

extern void pivot_stack(u32 stack_top);

void main();

__attribute__((noreturn)) void ipl_main(){
	hw_init();
	pivot_stack(IPL_STACK_TOP);
	heap_init((void*)IPL_HEAP_START);

	mc_enable_ahb_redirect();

	main();

	emmc_end();
	sd_end();
	hw_deinit(false, 0);

	rcm_if_t210_or_off();
	// power_set_state(POWER_OFF);

	while(1){}
}
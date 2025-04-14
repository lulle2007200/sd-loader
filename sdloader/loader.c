#include <utils/types.h>
#include <memory_map.h>


#pragma GCC optimize ("O3")
static void reloc(void *dst, void *src, u32 size){
	u8 *src_8 = (u8*)src;
	u8 *dst_8 = (u8*)dst;
	if(dst_8 < src_8){
		for(u32 i = 0; i < size; i++){
			dst_8[i] = src_8[i];
		} 
	}else if(dst_8 > src_8){
		for(u32 i = size - 1; i < size; i--){
			dst_8[i] = src_8[i];
		}
	}
}

__attribute__((noreturn)) void reloc_and_start_payload(void *addr, u32 size){
	reloc((void*)PAYLOAD_LOAD_ADDR, addr, size);
	((void (*)()) PAYLOAD_LOAD_ADDR)();
	while(1){}
}
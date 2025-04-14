#include <sec/se_t210.h>
#include <soc/t210.h>

extern u8 __payload_start;
extern u8 __encrypt_start;
extern u8 __encrypt_end;

typedef struct _se_ll_t
{
	vu32 num;
	vu32 addr;
	vu32 size;
} se_ll_t;

static se_ll_t ll_src, ll_dst;
static se_ll_t *ll_src_ptr, *ll_dst_ptr; // Must be u32 aligned.

static inline void se_ll_init(se_ll_t *ll, u32 addr, u32 size)
{
	ll->num  = 0;
	ll->addr = addr;
	ll->size = size;
}

static inline void se_ll_set(se_ll_t *src, se_ll_t *dst)
{
	SE(SE_IN_LL_ADDR_REG)  = (u32)src;
	SE(SE_OUT_LL_ADDR_REG) = (u32)dst;
}

static inline void se_wait()
{
	while (!(SE(SE_INT_STATUS_REG) & SE_INT_OP_DONE)){}
}

static inline void se_execute(u32 op, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	ll_src_ptr = NULL;
	ll_dst_ptr = NULL;

	if (src)
	{
		ll_src_ptr = &ll_src;
		se_ll_init(ll_src_ptr, (u32)src, src_size);
	}

	if (dst)
	{
		ll_dst_ptr = &ll_dst;
		se_ll_init(ll_dst_ptr, (u32)dst, dst_size);
	}

	se_ll_set(ll_src_ptr, ll_dst_ptr);

	SE(SE_ERR_STATUS_REG) = SE(SE_ERR_STATUS_REG);
	SE(SE_INT_STATUS_REG) = SE(SE_INT_STATUS_REG);

	SE(SE_OPERATION_REG) = op;

	se_wait();
}

static inline void se_aes_iv_clear(u32 ks)
{
	for (u32 i = 0; i < (SE_AES_IV_SIZE / 4); i++)
	{
		SE(SE_CRYPTO_KEYTABLE_ADDR_REG) = SE_KEYTABLE_SLOT(ks) | SE_KEYTABLE_QUAD(ORIGINAL_IV) | SE_KEYTABLE_PKT(i);
		SE(SE_CRYPTO_KEYTABLE_DATA_REG) = 0;
	}
}

static inline void se_aes_decrypt_cbc(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	SE(SE_CONFIG_REG)        = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_MEMORY);
	SE(SE_CRYPTO_CONFIG_REG) = SE_CRYPTO_KEY_INDEX(ks)          | SE_CRYPTO_VCTRAM_SEL(VCTRAM_AESOUT) |
							   SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) | SE_CRYPTO_XOR_POS(XOR_TOP);
	SE(SE_CRYPTO_BLOCK_COUNT_REG) = (src_size >> 4) - 1;
	se_execute(SE_OP_START, dst, dst_size, src, src_size);
}

// brom decrypts the unencrypted part aswell, reencrypt it to restore plaintext
// loader has 0x20 zero bytes appendend, so after decryption, we have 0x10 zero bytes before payload
// so decryption of just the payload matches decryption with iv = 0
// we reencrypt *only* the payload with iv = 0
static inline void reencrypt_payload(){
	se_aes_iv_clear(13);
	void *addr = &__encrypt_start;
	u32 sz = (u32)(&__encrypt_end - &__encrypt_start);
	// decrypt payload in-place
	se_aes_decrypt_cbc(13, addr, sz, addr, sz);
}

static __attribute__((used)) void start(){
	reencrypt_payload();
	void (*ext_payload_ptr)() = (void *)&__payload_start;
	(*ext_payload_ptr)();
}

// entry point, set up sp and call start
__attribute__((naked, used, section(".text._start"), target("arm"))) 
void _start(){
	asm volatile(".arm"                        "\n\t"
	             "LDR SP, =0x4003ff00"         "\n\t"
	             "BL  start"                   "\n\t"
	             "loop:"                       "\n\t"
	             "B loop"                      "\n\t");
}
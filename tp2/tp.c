/* GPLv2 (c) Airbus */
#include <debug.h>
#include <intr.h>

void bp_handler() {
   // TODO
   // Question 2
   debug("On est dans le bp_handler\n");

   // Question 7
    uint32_t val;
   asm volatile ("mov 4(%%ebp), %0":"=r"(val));
}

void bp_trigger() {
	asm volatile ("int3");
}

void tp() {
	// TODO print idtr
	idt_reg_t idtr;
	get_idtr(idtr);
	printf("addr : 0x%lx\n",idtr.addr);

	int_desc_t * bp_dsc = & idtr.desc[3];
	bp_dsc->offset_1 = (uint16_t) ((uint32_t )bp_handler);
	bp_dsc -> offset_2 = (uint16_t) (((uint32_t )bp_handler) >> 16);

	

	// TODO call bp_trigger
   bp_trigger();
}

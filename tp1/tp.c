/* GPLv2 (c) Airbus */
#include <debug.h>
#include <segmem.h>


void userland() {
   asm volatile ("mov %eax, %cr0");
}


void print_gdt_content(gdt_reg_t gdtr_ptr) {
    seg_desc_t* gdt_ptr;
    gdt_ptr = (seg_desc_t*)(gdtr_ptr.addr);
    int i=0;
    while ((uint32_t)gdt_ptr < ((gdtr_ptr.addr) + gdtr_ptr.limit)) {
        uint32_t start = gdt_ptr->base_3<<24 | gdt_ptr->base_2<<16 | gdt_ptr->base_1;
        uint32_t end;
        if (gdt_ptr->g) {
            end = start + ( (gdt_ptr->limit_2<<16 | gdt_ptr->limit_1) <<12) + 4095;
        } else {
            end = start + (gdt_ptr->limit_2<<16 | gdt_ptr->limit_1);
        }
        debug("%d ", i);
        debug("[0x%x ", start);
        debug("- 0x%x] ", end);
        debug("seg_t: 0x%x ", gdt_ptr->type);
        debug("desc_t: %d ", gdt_ptr->s);
        debug("priv: %d ", gdt_ptr->dpl);
        debug("present: %d ", gdt_ptr->p);
        debug("avl: %d ", gdt_ptr->avl);
        debug("longmode: %d ", gdt_ptr->l);
        debug("default: %d ", gdt_ptr->d);
        debug("gran: %d ", gdt_ptr->g);
        debug("\n");
        gdt_ptr++;
        i++;
    }
}


void tp() {
	// TODO
    gdt_reg_t gdt;
    get_gdtr(gdt);
    print_gdt_content(gdt);

    printf("\n");

    printf("cs : 0X%x \n",get_cs());
    printf("ds : 0x%x \n",get_ds());    
    printf("ss : 0X%x \n",get_ss());
    printf("es : 0X%x \n",get_es());
    printf("fs : 0X%x \n",get_fs());
    printf("gs : 0X%x \n",get_ds());

     printf("---------------\n");
    
    seg_desc_t nouv_gdt[3];

    //Selecteur nul
    seg_desc_t seg_nul = (seg_desc_t) {0};

    // Code, 32 bits RX, flat, indice 1
    seg_desc_t nouv_seg_code;
    nouv_seg_code.limit_1 = 0xffff ;
    nouv_seg_code.base_1 = 0x0;
    nouv_seg_code.base_2 = 0x0;
    nouv_seg_code.type = 10;
    nouv_seg_code.s = 1;
    nouv_seg_code.dpl = 0;
    nouv_seg_code.p = 1;
    nouv_seg_code.limit_2 = 0x4;
    nouv_seg_code.avl= 0;
    nouv_seg_code.l= 0;
    nouv_seg_code.d = 1;
    nouv_seg_code.g = 0;
    nouv_seg_code.base_3 = 0x0 ;

    // Données, 32 bits RW, flat, indice 2.
    seg_desc_t nouv_seg_data;
    nouv_seg_data.limit_1 = 0xffff ;
    nouv_seg_data.base_1 = 0x0;
    nouv_seg_data.base_2 = 0x0;
    nouv_seg_data.type = 2;
    nouv_seg_data.s = 1;
    nouv_seg_data.dpl = 0;
    nouv_seg_data.p = 1;
    nouv_seg_data.limit_2 = 0x4;
    nouv_seg_data.avl= 0;
    nouv_seg_data.l= 0;
    nouv_seg_data.d = 1;
    nouv_seg_data.g = 0;
    nouv_seg_data.base_3 = 0x0 ;


    nouv_gdt[0] = seg_nul;
    nouv_gdt[1] = nouv_seg_code;
    nouv_gdt[2] = nouv_seg_data;

    gdt_reg_t nouv_gdtr;
    nouv_gdtr.limit = 0xF;
    nouv_gdtr.addr = 0x0;
    nouv_gdtr.desc = nouv_gdt;

    seg_sel_t sel_data;
    sel_data.rpl =0;
    sel_data.ti = 0;
    sel_data.index = 2;
     set_ss(sel_data);
    set_gdtr(nouv_gdtr);
    
    get_gdtr(nouv_gdtr);
    print_gdt_content(nouv_gdtr);
    printf("ss : 0X%x \n",get_ss());



/*
    printf("cs : 0X%x \n",get_cs());
    printf("ds : 0x%x \n",get_ds());    
    printf("ss : 0X%x \n",get_ss());
    printf("es : 0X%x \n",get_es());
    printf("fs : 0X%x \n",get_fs());
    printf("gs : 0X%x \n",get_ds());
*/


    

}

/* GPLv2 (c) Airbus */
#include <debug.h>
#include <info.h>
#include <asm.h>
#include <segmem.h>
#include <pagemem.h>
#include <intr.h>
#include <types.h>
#include <cr.h>

/*
tss_t tss;
idt_reg_t idtr;
uint32_t tasks[4] = {};
unsigned int number_tasks = 0;
int current_task = -1;
*/


int task_encours = 0;

// Segmentation

#define c0_idx  1
#define d0_idx  2
#define c3_idx  3
#define d3_idx  4
#define ts_idx  5

#define c0_sel  gdt_krn_seg_sel(c0_idx)
#define d0_sel  gdt_krn_seg_sel(d0_idx)
#define c3_sel  gdt_usr_seg_sel(c3_idx)
#define d3_sel  gdt_usr_seg_sel(d3_idx)
#define ts_sel  gdt_krn_seg_sel(ts_idx)

seg_desc_t GDT[6];
tss_t      TSS;

#define gdt_flat_dsc(_dSc_,_pVl_,_tYp_)                                 \
   ({                                                                   \
      (_dSc_)->raw     = 0;                                             \
      (_dSc_)->limit_1 = 0xffff;                                        \
      (_dSc_)->limit_2 = 0xf;                                           \
      (_dSc_)->type    = _tYp_;                                         \
      (_dSc_)->dpl     = _pVl_;                                         \
      (_dSc_)->d       = 1;                                             \
      (_dSc_)->g       = 1;                                             \
      (_dSc_)->s       = 1;                                             \
      (_dSc_)->p       = 1;                                             \
   })

#define tss_dsc(_dSc_,_tSs_)                                            \
   ({                                                                   \
      raw32_t addr    = {.raw = _tSs_};                                 \
      (_dSc_)->raw    = sizeof(tss_t);                                  \
      (_dSc_)->base_1 = addr.wlow;                                      \
      (_dSc_)->base_2 = addr._whigh.blow;                               \
      (_dSc_)->base_3 = addr._whigh.bhigh;                              \
      (_dSc_)->type   = SEG_DESC_SYS_TSS_AVL_32;                        \
      (_dSc_)->p      = 1;                                              \
   })

#define c0_dsc(_d) gdt_flat_dsc(_d,0,SEG_DESC_CODE_XR)
#define d0_dsc(_d) gdt_flat_dsc(_d,0,SEG_DESC_DATA_RW)
#define c3_dsc(_d) gdt_flat_dsc(_d,3,SEG_DESC_CODE_XR)
#define d3_dsc(_d) gdt_flat_dsc(_d,3,SEG_DESC_DATA_RW)

void init_gdt() {
   gdt_reg_t gdtr;

   GDT[0].raw = 0ULL;

   c0_dsc( &GDT[c0_idx] );
   d0_dsc( &GDT[d0_idx] );
   c3_dsc( &GDT[c3_idx] );
   d3_dsc( &GDT[d3_idx] );

   gdtr.desc  = GDT;
   gdtr.limit = sizeof(GDT) - 1;
   set_gdtr(gdtr);

   set_cs(c0_sel);

   set_ss(d0_sel);
   set_ds(d0_sel);
   set_es(d0_sel);
   set_fs(d0_sel);
   set_gs(d0_sel);
}


// Pagination

#define SHARED_PAGE 0x1000000
#define PTB_KERNEL 0x702000
#define PTB_USER1 0x703000
#define PTB_USER2 0x704000
pde32_t *pgd_task1 = (pde32_t*)0x700000;
pde32_t *pgd_task2 = (pde32_t*)0x701000;

// identity map toutes les pages de la PTB
void idmap_ptb (pte32_t *ptb, uint32_t first_index_page, uint32_t perms) {
  memset((void*)ptb, 0, 1024);
  for (size_t i = 0; i < 1024; i++){
    pg_set_entry(&ptb[i], perms, (uint32_t)first_index_page  + i);
  }
}



void set_paging(){

  //Definir 2 pages directories vides pour chaque tâhce
  memset((void*)pgd_task1, 0, 1024);
  memset((void*)pgd_task2, 0, 1024);

  // Definition des pages tables
  pte32_t* ptb[3];
  ptb[0] = (pte32_t*) PTB_KERNEL;
  ptb[1] =  (pte32_t*)PTB_USER1;
  ptb[2] = (pte32_t*) PTB_USER2;

  // On fait l'identity map
  for(int i=0; i<3; i++){
    if (i== 0) {
      idmap_ptb(ptb[i], i * (1 << 10),PG_KRN | PG_RW);
    } else {
      idmap_ptb(ptb[i], i * (1 << 10), PG_USR | PG_RW); 
    }
    
  }
  

  // definir espace virtuel task1
  pg_set_entry(&pgd_task1[0],PG_KRN | PG_RW, pg_4K_get_nr((uint32_t)ptb[0])); //Kernel
  pg_set_entry(&pgd_task1[1],PG_USR | PG_RW, pg_4K_get_nr((uint32_t)ptb[1])); 
  pg_set_entry(&ptb[1][1023], PG_USR | PG_RW, pg_4K_get_nr((uint32_t) SHARED_PAGE)); // page partagée (dans la dernière page)

  // definir espace virtuel task1: ptbs kernel + ptb task2
  pg_set_entry(&pgd_task2[0],PG_KRN | PG_RW, pg_4K_get_nr((uint32_t)ptb[0])); //Kernel
  pg_set_entry(&pgd_task2[1],PG_USR | PG_RW, pg_4K_get_nr((uint32_t)ptb[2]));
    pg_set_entry(&ptb[2][1023], PG_USR | PG_RW, pg_4K_get_nr((uint32_t) SHARED_PAGE)); // page partagée (dans la dernière page)

  set_cr3((uint32_t)pgd_task1); // on commence dans la task 1
}



void tp() {

   // Lancement de la segmentation
   init_gdt();

   // Pagination
   set_paging();
   set_cr0(get_cr0() | CR0_PG | CR0_PE);


    

    while(1){
      //scheduler();

    }
}

/* GPLv2 (c) Airbus */
#include <debug.h>
#include <info.h>
#include <asm.h>
#include <segmem.h>
#include <pagemem.h>
#include <intr.h>
#include <types.h>
#include <cr.h>


// Projet de FALLOT Ysabel


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
void identity_mapping (pte32_t *ptb, uint32_t first_index_page, uint32_t permissions) {
  memset((void*)ptb, 0, 1024);
  for (size_t i = 0; i < 1024; i++){
    pg_set_entry(&ptb[i], permissions, (uint32_t)first_index_page  + i);
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
      identity_mapping(ptb[i], i * (1 << 10),PG_KRN | PG_RW);
    } else {
      identity_mapping(ptb[i], i * (1 << 10), PG_USR | PG_RW); 
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



// Gestion des tâches
int task_encours;
uint32_t *shared_mem = (uint32_t *)SHARED_PAGE;

// Tâche 1 qui incrémente le compteur dans la mémoire partagée
__attribute__((section(".user1"))) void user1(){
   debug("On est dans tâche 1\n");
    *shared_mem = 0;
    while (1) {
      (*shared_mem)++;
}
}

// Fonction utilisée par la tâche 2 pour appeler le syscall qui affiche la mémoire partagée
__attribute__((section(".sys_count"))) void sys_counter(uint32_t*counter){
    asm volatile("int $80"::"S"(counter));
}

// Tâche 2 qui appelle sys_counter pour afficher la valeur
__attribute__((section(".user2"))) void user2(){
   debug("On est dans tâche 2\n");
    while (1) {
      sys_counter(shared_mem);
    }
}



// Appel système qui affoche la valeur
// TO DO : à faire
void syscall(){
    debug("Syscall interruption (80): Appel de User2 \n");
}


// Fonction sauvegardant le contexte avant de changer de tâches
void scheduler(){
    debug("Record contexte.\n");
    asm("handler_scheduler:\n");
    asm("pusha\n");
    asm("call change_task\n");
    asm("popa\n");
    // Return traiter l’interruption 3/ Restaurer les registres (popa), 4/ Retourner là où on était (iret)
    asm("iret\n");

    asm("call scheduler\n");
  }



// Fonction pour changer de tâches
void change_task(){
    debug("CHangement de tâches.\n");
    if (task_encours != 1) {
      // On passe dans la tâche 1
      debug("Task 1:\n");
      task_encours=1;
      asm("call user1");
    }
    else {
        task_encours=2;
        debug("Task 2:\n");
        // On appelle l'interruption de la tâche 2
        asm("call user2");
    }
}


//Fonction créant l'IDT et ajoutant les handlers associés
void set_interrupt(){
    // Définition de l'IDT
    idt_reg_t idtr;
    get_idtr(idtr);
    int_desc_t *idt = idtr.desc;

    // Ajout du handler lié au scheduler
    build_int_desc(&idt[32], c0_sel, (offset_t)scheduler);
    idt[32].dpl = SEG_SEL_USR;

    // Ajout du handler syscall (80)
    build_int_desc(&idt[0x80], c0_sel, (offset_t)syscall);
    idt[0x80].dpl = SEG_SEL_USR; 
}


void tp() {

   // Lancement de la segmentation
   init_gdt();

   // Pagination
   set_paging();
   set_cr0(get_cr0() | CR0_PG | CR0_PE);

   // Interruption
   set_interrupt();

   task_encours = 1;

   //Lancement du changement tâches
   scheduler(); // Nécessaire ? Pas censé être sur le TIC d'horloge
    

    while(1){

    }
}
/*
Plusieurs problèmes identifiés :
- Page Fault : origine probable : 
        - l'accès à la mémoire partagé pour incrémenter le compteur
        - mauvaise sauvegarde de contexte lors du changement de tâche

- Tâche 2 non testé : problème : lorsque dans la fonction tp() je mets la tâche courante à 2, on semble être dans la fonction 1 (mais le "code" (les erreurs à la suite) s'arrête)


*/
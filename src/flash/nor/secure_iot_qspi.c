#include"secure_iot_qspi.h"

uint32_t QSPI_Transaction(struct target *target,uint32_t instance_number,qspi_msg *msg){
   if(instance_number>1)
     return -1;
   // if(msg->length>16)
   //   return -16;
   uint32_t qspi_base = 0x40000+(instance_number*0x100);
   uint32_t cr_address = qspi_base+0x00;
   uint32_t dcr_address = qspi_base+0x04;
   uint32_t sr_address = qspi_base+0x08;
   uint32_t fcr_address = qspi_base+0x0c;
   uint32_t dlr_address = qspi_base+0x10;
   uint32_t ccr_address = qspi_base+0x14;
   uint32_t ar_address = qspi_base+0x18;
   uint32_t abr_address = qspi_base+0x1c;
   uint32_t dr_address = qspi_base+0x20;

   target_write_u32(target, cr_address,(CR_PRESCALER(msg->PRESCALER) |CR_PMM(msg->PMM) | CR_APMS(msg->APMS) | CR_TOIE(msg->TOIE) |CR_SMIE(msg->SMIE) | CR_FTIE(msg->FTIE) | CR_TCIE(msg->TCIE) | CR_TEIE(msg->TEIE) | CR_TCEN(msg->TOIE) | CR_EN(1)));         
   target_write_u32(target, dcr_address,(DCR_FSIZE(msg->FMEM_SIZE) | DCR_CKMODE(msg->CLK_MODE)));    
   uint32_t temp;
   do{
   //     temp = QUADSPI_Reg(instance_number)->SR;
   target_read_u32(target,sr_address,&temp);
       temp &= SR_BUSY;
   }while(temp==SR_BUSY);//check for busy status

   target_write_u32(target, fcr_address,(FCR_CTOF|FCR_CSMF|FCR_CTCF|FCR_CTEF));//clear flags
   target_write_u32(target, dlr_address,msg->length);
   temp = (CCR_INSTRUCTION(msg->instruction) | CCR_IMODE(msg->instruction_mode) | CCR_ADMODE(msg->address_mode) | CCR_ADSIZE(msg->address_size) |\
   CCR_ABMODE(msg->alternate_byte_mode) | CCR_ABSIZE(msg->sioo) | CCR_DCYC(msg->dummy_cycles) | CCR_DUMMY_CONFIRMATION(msg->dummy_mode) |\
   CCR_DMODE(msg->data_mode) | CCR_FMODE(msg->functional_mode) | CCR_SIOO(msg->sioo) | CCR_DUMMY_BIT(msg->dummy_bit) | CCR_MM_MODE(msg->mm_mode));
   target_write_u32(target, ccr_address,temp);
   target_write_u32(target, ar_address,msg->address);
   target_write_u32(target, abr_address,msg->alternate_byte);

   uint8_t i = 0;
   uint32_t status_reg;
   uint32_t cr_value;
   if(msg->functional_mode == CCR_FMODE_INDIRECT_WRITE && msg->length!= 0)
   {
   //   QUADSPI_Reg(instance_number)->CR&= ~(CR_FTHRES(15));
     target_read_u32(target,cr_address,&cr_value);
     cr_value&=~(CR_FTHRES(15));
     target_write_u32(target, cr_address,cr_value);
     while(1){
       target_read_u32(target,sr_address,&status_reg);
       //status_reg = QUADSPI_Reg(instance_number)->SR;
       status_reg &= SR_FTF;
       if(status_reg){
           target_write_u8(target,dr_address,msg->data_buffer[i]);
         i++;
           if(i == msg->length)
             break;
       }
     }
     do{
       //   temp = QUADSPI_Reg(instance_number)->SR;
       target_read_u32(target,sr_address,&temp);
         temp&=SR_FLEVEL(FIFO_EMPTY);
       }while(temp != 0);
       
   }
   else if(msg->functional_mode == CCR_FMODE_INDIRECT_READ)
   {
   //   QUADSPI_Reg(instance_number)->CR&= ~(CR_FTHRES(15));
   target_read_u32(target,cr_address,&cr_value);
   cr_value&=~(CR_FTHRES(15));
   target_write_u32(target, cr_address,cr_value);
     while(1){
       // status_reg = QUADSPI_Reg(instance_number)->SR;
       target_read_u32(target,sr_address,&status_reg);
       status_reg &= SR_FTF;
       if(status_reg){
           target_read_u8(target,dr_address,&msg->data_buffer[i]);
       //   msg->data_buffer[i] = QUADSPI_Reg(instance_number)->DR.data_8;
         i++;
           if(i == msg->length)
             break;
       }
     }
   }
   if(msg->functional_mode == CCR_FMODE_MMM && msg->mm_mode == CCR_MM_MODE_RAM){
   //   QUADSPI_Reg(instance_number)->CR&=~(CR_FTHRES(15));
   target_read_u32(target,cr_address,&cr_value);
   cr_value&=~(CR_FTHRES(15));
   cr_value|=(CR_FTHRES(msg->fthresh));
   target_write_u32(target, cr_address,cr_value);
     return 0;
   }
   if(msg->functional_mode == CCR_FMODE_MMM && msg->mm_mode == CCR_MM_MODE_XIP){
   //   QUADSPI_Reg(instance_number)->CR&=~(CR_FTHRES(15));
   target_read_u32(target,cr_address,&cr_value);
   cr_value&=~(CR_FTHRES(15));
   target_write_u32(target, cr_address,cr_value);
     return 0;
   }

   if(msg->data_mode == CCR_DMODE_NO_DATA)
   {
     do{
   //   temp = QUADSPI_Reg(instance_number)->SR;
   target_read_u32(target,sr_address,&temp);
     temp &= SR_TCF;
   }while(temp == 0);
   }
  //  usleep(100); 
   target_write_u32(target, cr_address,0x00);
   return 0;
}
#include "secure_iot_qspi.h"
#include <helper/log.h>
// #define DBG_PRINT(...) LOG_INFO(__VA_ARGS__)
// or use printf if needed

#define WAIT_TIMEOUT 1000000
uint32_t QSPI_Transaction(struct target *target, uint32_t instance_number, qspi_msg *msg)
{
  if (instance_number > 1)
    return -1;
  // if(msg->length>16)
  //   return -16;
  uint32_t qspi_base = 0x60200 + (instance_number * 0x100);
  uint32_t cr_address = qspi_base + 0x00;
  uint32_t dcr_address = qspi_base + 0x04;
  uint32_t sr_address = qspi_base + 0x08;
  uint32_t fcr_address = qspi_base + 0x0c;
  uint32_t dlr_address = qspi_base + 0x10;
  uint32_t ccr_address = qspi_base + 0x14;
  uint32_t ar_address = qspi_base + 0x18;
  uint32_t abr_address = qspi_base + 0x1c;
  uint32_t dr_address = qspi_base + 0x20;

  target_write_u32(target, cr_address, (CR_PRESCALER(msg->PRESCALER) | CR_PMM(msg->PMM) | CR_APMS(msg->APMS) | CR_TOIE(msg->TOIE) | CR_SMIE(msg->SMIE) | CR_FTIE(msg->FTIE) | CR_TCIE(msg->TCIE) | CR_TEIE(msg->TEIE) | CR_TCEN(msg->TOIE) | CR_EN(1)));
  uint32_t cr_value;
  target_read_u32(target, cr_address, &cr_value);
  cr_value &= ~CR_FTHRES(15);
  cr_value |= CR_FTHRES(0);
  target_write_u32(target, cr_address, cr_value);
  target_write_u32(target, dcr_address, (DCR_FSIZE(msg->FMEM_SIZE) | DCR_CKMODE(msg->CLK_MODE)));
  uint32_t temp;
  do
  {
    target_read_u32(target, sr_address, &temp);
    temp &= SR_BUSY;
  } while (temp == SR_BUSY); // check for busy status

  target_write_u32(target, fcr_address, (FCR_CTOF | FCR_CSMF | FCR_CTCF | FCR_CTEF)); // clear flags
  target_write_u32(target, dlr_address, (msg->length - 1));
  temp = (CCR_INSTRUCTION(msg->instruction) | CCR_IMODE(msg->instruction_mode) | CCR_ADMODE(msg->address_mode) | CCR_ADSIZE(msg->address_size) |
          CCR_ABMODE(msg->alternate_byte_mode) | CCR_ABSIZE(msg->sioo) | CCR_DCYC(msg->dummy_cycles) | CCR_DUMMY_CONFIRMATION(msg->dummy_mode) |
          CCR_DMODE(msg->data_mode) | CCR_FMODE(msg->functional_mode) | CCR_SIOO(msg->sioo) | CCR_DUMMY_BIT(msg->dummy_bit) | CCR_MM_MODE(msg->mm_mode));
  target_write_u32(target, ccr_address, temp);
  target_write_u32(target, ar_address, msg->address);
  target_write_u32(target, abr_address, msg->alternate_byte);

  uint8_t i = 0;
  uint32_t status_reg;

  // uint32_t temp;
  if (msg->functional_mode == CCR_FMODE_INDIRECT_WRITE && msg->length != 0)
  {
    uint64_t *word_64 = (uint64_t *)msg->data_buffer;
    target_read_u32(target, cr_address, &cr_value);
    cr_value &= ~CR_FTHRES(15);
    cr_value |= CR_FTHRES(7);
    target_write_u32(target, cr_address, cr_value);
    while ((msg->length - i) >= 8)
    {
      // wait until FIFO has space
      do
      {
        target_read_u32(target, sr_address, &status_reg);
      } while (!(status_reg & SR_FTF));
      target_write_u64(target, dr_address, *word_64);
      word_64++;
      i += 8;
    }

    //
    // ---------------- 32-bit writes ----------------
    //
    uint32_t *word_32 = (uint32_t *)word_64;
    target_read_u32(target, cr_address, &cr_value);
    cr_value &= ~CR_FTHRES(15);
    cr_value |= CR_FTHRES(3);
    target_write_u32(target, cr_address, cr_value);
    while ((msg->length - i) >= 4)
    {
      // wait until FIFO has space
      do
      {
        target_read_u32(target, sr_address, &status_reg);
      } while (!(status_reg & SR_FTF));
      target_write_u32(target, dr_address, *word_32);
      word_32++;
      i += 4;
    }

    //
    // ---------------- 16-bit writes ----------------
    //

    uint16_t *word_16 = (uint16_t *)word_32;
    target_read_u32(target, cr_address, &cr_value);
    cr_value &= ~CR_FTHRES(15);
    cr_value |= CR_FTHRES(1);
    target_write_u32(target, cr_address, cr_value);
    while ((msg->length - i) >= 2)
    {
      // wait until FIFO has space
      do
      {
        target_read_u32(target, sr_address, &status_reg);
      } while (!(status_reg & SR_FTF));
      target_write_u16(target, dr_address, *word_16);
      word_16++;
      i += 2;
    }

    //
    // ---------------- 8-bit writes ----------------
    //
    uint8_t *word_8 = (uint8_t *)word_16;
    target_read_u32(target, cr_address, &cr_value);
    cr_value &= ~CR_FTHRES(15);
    cr_value |= CR_FTHRES(0);
    target_write_u32(target, cr_address, cr_value);
    while ((msg->length - i) >= 1)
    {
      // wait until FIFO has space
      do
      {
        target_read_u32(target, sr_address, &status_reg);
      } while (!(status_reg & SR_FTF));
      target_write_u16(target, dr_address, *word_8);
      word_8++;
      i += 1;
    }
    do
    {
      target_read_u32(target, sr_address, &temp);
      temp &= SR_FLEVEL(FIFO_EMPTY);  
    } while (temp != 0);
  }
  else if (msg->functional_mode == CCR_FMODE_INDIRECT_READ)
  {
    //   QUADSPI_Reg(instance_number)->CR&= ~(CR_FTHRES(15));
    target_read_u32(target, cr_address, &cr_value);
    cr_value &= ~(CR_FTHRES(15));
    target_write_u32(target, cr_address, cr_value);
    while (1)
    {
      // status_reg = QUADSPI_Reg(instance_number)->SR;
      target_read_u32(target, sr_address, &status_reg);
      status_reg &= SR_FTF;
      if (status_reg)
      {
        target_read_u8(target, dr_address, &msg->data_buffer[i]);
        //   msg->data_buffer[i] = QUADSPI_Reg(instance_number)->DR.data_8;
        i++;
        if (i == msg->length)
          break;
      }
    }
  }
  if (msg->functional_mode == CCR_FMODE_MMM && msg->mm_mode == CCR_MM_MODE_RAM)
  {
    //   QUADSPI_Reg(instance_number)->CR&=~(CR_FTHRES(15));
    target_read_u32(target, cr_address, &cr_value);
    cr_value &= ~(CR_FTHRES(15));
    cr_value |= (CR_FTHRES(msg->fthresh));
    target_write_u32(target, cr_address, cr_value);
    return 0;
  }
  if (msg->functional_mode == CCR_FMODE_MMM && msg->mm_mode == CCR_MM_MODE_XIP)
  {
    //   QUADSPI_Reg(instance_number)->CR&=~(CR_FTHRES(15));
    target_read_u32(target, cr_address, &cr_value);
    cr_value &= ~(CR_FTHRES(15));
    target_write_u32(target, cr_address, cr_value);
    return 0;
  }

  if (msg->data_mode == CCR_DMODE_NO_DATA)
  {
    do
    {
      //   temp = QUADSPI_Reg(instance_number)->SR;
      target_read_u32(target, sr_address, &temp);
      temp &= SR_TCF;
    } while (temp == 0);
  }
  //  usleep(100);
  target_write_u32(target, cr_address, 0x00);
  return 0;
}
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include<elf.h>

#include"secure_iot_flash_driver.h"
#define CHUNK_SIZE 16
qspi_msg flash_msg={.PRESCALER=6,.CLK_MODE=0,.FMEM_SIZE = 27,.FTIE = 0,.TCEN=0,.TEIE=0,.TOIE=0,.SMIE = 0,.APMS= 0,.PMM=0};


/**
 * @fn uint32_t fastReadQuad(struct target *target,uint8_t qspinum,uint8_t* data,uint32_t address,uint8_t data_length)
 * 
 * @brief Used to read data from flash through all the four data lines and instruction through one address lines.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param data The parameter \a data is a pointer to array which contains data to be read from flash.
 * @param address The parameter \a address specifies from which start address data to be read from flash through QSPI.
 * @param data_length The parameter \a data_length specifies how many bytes of data should be read from flash.
 * 
 * @return SUCCESS if operation is successful,ENODEV if invalid instance number and ELENEXCEED is length of read and write parameters exceeded.
 */
void print_progress_bar(int progress, int total) {
    int bar_width = 50;  // Width of the progress bar
    float progress_percentage = (float)progress / total;
    int filled = (int)(progress_percentage * bar_width);

    // Print the progress bar
    log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "\r[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "#");  // Filled part of the bar
        } else {
            log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, " ");  // Empty part of the bar
        }
    }
    log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "] %.2f%%", progress_percentage * 100);
    // fflush(stdout);  // Ensure that the output is immediately displayed
}
uint32_t fastReadQuad(struct target *target,uint8_t qspinum,uint8_t* data,uint32_t address,uint8_t data_length){
    flash_msg.address = address;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x6B;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_FOUR_LINE;
    flash_msg.data_buffer = data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 7;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = data_length;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}

/**
 * @fn uint32_t fastReadQuadIO(struct target *target,uint8_t qspinum,uint8_t *data,uint32_t address,uint8_t data_length)
 * 
 * @brief Used to write data into flash through all the four data lines and instruction through two address lines.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param data The parameter \a data is a pointer to array which contains data to written in flash.
 * @param address The parameter \a address specifies from which start address data to be read from flash through QSPI.
 * @param data_length The parameter \a data_length specifies how many bytes of data should be written to flash from array.
 * 
 * @return SUCCESS if operation is successful,ENODEV if invalid instance number and ELENEXCEED is length of read and write parameters exceeded.
 */
uint32_t fastReadQuadIO(struct target *target,uint8_t qspinum,uint8_t *data,uint32_t address,uint8_t data_length){
    flash_msg.address = address;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0xEB;
    flash_msg.instruction_mode = CCR_IMODE_TWO_LINE;
    flash_msg.data_mode = CCR_DMODE_FOUR_LINE;
    flash_msg.data_buffer = data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 4;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_FOUR_LINE;
    flash_msg.alternate_byte = 0x20;
    flash_msg.length = data_length;
    return QSPI_Transaction(target,qspinum,&flash_msg);
    
}
/**
 * @fn uint32_t fastReadSingle(struct target *target,uint8_t qspinum,uint8_t *data,uint32_t address,uint8_t data_length)
 * 
 * @brief Used to write data into flash through one data line and one instruction line.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param data The parameter \a data is a pointer to array which contains data read from flash.
 * @param address The parameter \a address specifies from which start address data should be read from flash.
 * @param data_length The parameter \a data_length specifies how many bytes of data should be read from flash.
 * 
 * @return SUCCESS if operation is successful,ENODEV if invalid instance number and ELENEXCEED is length of read and write parameters exceeded.
 */
uint32_t fastReadSingle(struct target *target,uint8_t qspinum,uint8_t *data,uint32_t address,uint8_t data_length){
    flash_msg.address = address;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x03;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.data_buffer = data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = data_length;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}

/**
 * @fn uint32_t inputpageQuad(struct target *target,uint8_t qspinum,uint8_t* data,uint32_t address,uint8_t data_length)
 * 
 * @brief Used to write data to flash with one address line,one instruction line and four data lines.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param data The parameter \a data is a pointer to array which contains data to be read from flash.
 * @param address The parameter \a address specifies from which start address data should be read from flash.
 * @param data_length The parameter \a data_length specifies how many bytes of data should be read from flash.
 * 
 * @return SUCCESS if operation is successful,ENODEV if invalid instance number and ELENEXCEED is length of read and write parameters exceeded.
 */
uint32_t inputpageQuad(struct target *target,uint8_t qspinum,uint8_t* data,uint32_t address,uint8_t data_length){
    flash_msg.address = address;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x32;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_FOUR_LINE;
    flash_msg.data_buffer = data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = data_length;
    return QSPI_Transaction(target,qspinum,&flash_msg);
    
}

/**
 * @fn uint32_t inputpageSingle(struct target *target,uint8_t qspinum,uint8_t* data,uint32_t address,uint8_t data_length)
 * 
 * @brief Used to write to flash through single line.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param data The parameter \a data is a pointer to array which contains data to be written to flash.
 * @param address The parameter \a address specifies from which start address data to be written to flash through QSPI.
 * @param data_length The parameter \a data_length specifies how many bytes of data should be written to flash from array.
 * 
 * @return SUCCESS if operation is successful,ENODEV if invalid instance number and ELENEXCEED is length of read and write parameters exceeded.
 */
uint32_t inputpageSingle(struct target *target,uint8_t qspinum,uint8_t* data,uint32_t address,uint8_t data_length){
    flash_msg.address = address;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x02;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_FOUR_LINE;
    flash_msg.data_buffer = data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = data_length;
    return QSPI_Transaction(target,qspinum,&flash_msg);
    
}
/**
 * @fn void sector4KErase(struct target *target,uint8_t qspinum,uint32_t address)
 * 
 * @brief Used to erase QSPI flash for 4KB of space from mentioned starting address.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param address The parameter \a address specifies from which start address data should be erased from flash through QSPI.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t sector4KErase(struct target *target,uint8_t qspinum,uint32_t address){
    flash_msg.address = address;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x20;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
	QSPI_Transaction(target,qspinum,&flash_msg);
	uint8_t temp;
    while(1){
        temp = readStatusRegister1(target,qspinum);
        temp = temp & 0x01;
        if(temp != 0x01)
        break;
    }
    return SUCCESS;
    
}
/**
 * @fn void sector32KErase(struct target *target,uint8_t qspinum,uint32_t address)
 * 
 * @brief Used to erase QSPI flash for 32KB of space from mentioned starting address.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param address The parameter \a address specifies from which start address data should be erased from flash through QSPI.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t sector32KErase(struct target *target,uint8_t qspinum,uint32_t address){
    flash_msg.address = address;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x52;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
	QSPI_Transaction(target,qspinum,&flash_msg);    
    uint8_t temp;
    while(1){
        temp = readStatusRegister1(target,qspinum);
        temp = temp & 0x01;
        if(temp != 0x01)
        break;
    }
    return SUCCESS;
}
/**
 * @fn void chipErase(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to erase whole chip through QSPI.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t chipErase(struct target *target,uint8_t qspinum){
    //currently not working so used 32Kchip erase as patch
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_8_BIT;
    flash_msg.instruction = 0xC7;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn void writeEnable(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to enable qspi write transactions.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeEnable(struct target *target,uint8_t qspinum){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_8_BIT;
    flash_msg.instruction = 0x06;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    QSPI_Transaction(target,qspinum,&flash_msg);
    uint8_t temp;
    while(1){
        temp = readStatusRegister1(target,qspinum);
        temp = temp & 0x02;
        if(temp == 0x02)
        break;
    }
    return SUCCESS;

}
/**
 * @fn void writeDisable(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to disable qspi write transactions.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeDisable(struct target *target,uint8_t qspinum){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_8_BIT;
    flash_msg.instruction = 0x04;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;  
    return QSPI_Transaction(target,qspinum,&flash_msg);
    
}
/**
 * @fn void suspend(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to suspend current qspi transactions.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t suspend(struct target *target,uint8_t qspinum){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_8_BIT;
    flash_msg.instruction = 0x75;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}


/**
 * @fn void resume(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to resume suspended qspi transactions.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t resume(struct target *target,uint8_t qspinum){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_8_BIT;
    flash_msg.instruction = 0x7A;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint32_t power_down(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to enter power down mode.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t power_down(struct target *target,uint8_t qspinum){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_8_BIT;
    flash_msg.instruction = 0xB9;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint32_t release_power_down(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to exit power down mode.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t release_power_down(struct target *target,uint8_t qspinum){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_8_BIT;
    flash_msg.instruction = 0xAB;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint8_t readStatusRegister1(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to read value in Status register 1.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return Returns value in Status Register 1.
 */
uint8_t readStatusRegister1(struct target *target,uint8_t qspinum){// status reg 1
    uint8_t data;
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x05;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.data_buffer = &data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    QSPI_Transaction(target,qspinum,&flash_msg);
	return data;
}
/**
 * @fn uint8_t readStatusRegister2(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to read value in Status register 2.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return Returns value in Status Register 2.
 */
uint8_t readStatusRegister2(struct target *target,uint8_t qspinum){// status reg 1
    uint8_t data;
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x35;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.data_buffer = &data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    QSPI_Transaction(target,qspinum,&flash_msg);
	return data;
}
/**
 * @fn uint8_t readStatusRegister3(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to read value in Status register 3.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return Returns value in Status Register 3.
 */
uint8_t readStatusRegister3(struct target *target,uint8_t qspinum){// status reg 1
    uint8_t data;
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x15;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.data_buffer = &data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    QSPI_Transaction(target,qspinum,&flash_msg);
	return data;
}
/**
 * @fn uint8_t readFlagStatusRegister(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to read value in Flag Status Register.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return Returns value in Flag Status Register.
 */
uint8_t readFlagStatusRegister(struct target *target,uint8_t qspinum){
    uint8_t data;
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x70;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.data_buffer = &data;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    QSPI_Transaction(target,qspinum,&flash_msg);
	return data;
}
/**
 * @fn void writeEnableStatusRegister(struct target *target,uint8_t qspinum)
 * 
 * @brief Used to enable write operation to status register.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeEnableStatusRegister(struct target *target,uint8_t qspinum){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.instruction = 0x50;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_NO_DATA;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 0;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn void writeStatusRegister1(struct target *target,uint8_t qspinum,uint8_t* statusData)
 * 
 * @brief Used to write value in Status Register 1.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param statusData The parameter \a statusData is value that has to be written to status register.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeStatusRegister1(struct target *target,uint8_t qspinum,uint8_t* statusData){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.instruction = 0x01;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    flash_msg.data_buffer = statusData;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn void writeStatusRegister2(struct target *target,uint8_t qspinum,uint8_t* statusData)
 * 
 * @brief Used to write value in Status Register 2.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param statusData The parameter \a statusData is value that has to be written to status register.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeStatusRegister2(struct target *target,uint8_t qspinum,uint8_t* statusData){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.instruction = 0x31;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    flash_msg.data_buffer = statusData;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn void writeStatusRegister3(struct target *target,uint8_t qspinum,uint8_t* statusData)
 * 
 * @brief Used to write value in Status Register 2.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param statusData The parameter \a statusData is value that has to be written to status register.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeStatusRegister3(struct target *target,uint8_t qspinum,uint8_t* statusData){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.instruction = 0x11;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    flash_msg.data_buffer = statusData;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint8_t readGlobalFreezeBit(struct target *target,uint8_t qspinum)
 *
 * @brief Used to read global freeze bit.
 *
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 *
 * @return 1 if global freeze bit is set otherwise 0.
 */
uint8_t readGlobalFreezeBit(struct target *target,uint8_t qspinum){
    uint8_t data;
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0xA7;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    flash_msg.data_buffer = &data;
    QSPI_Transaction(target,qspinum,&flash_msg);
	return data;
}
/**
 * @fn uint32_t writeGlobalFreezeBit(struct target *target,uint8_t qspinum)
 *
 * @brief Used to set or clear global freeze bit.
 *
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 *
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeGlobalFreezeBit(struct target *target,uint8_t qspinum,uint8_t *data){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0xA6;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 1;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.data_buffer = data;
    flash_msg.length = 1;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint8_t readFlashSFDP(struct target *target,uint8_t qspinum,uint32_t address)
 * 
 * @brief Used to read value of Serial Flash Discoverable Parameter.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param address The parameter \a address specifies from which address we have to read data.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint8_t readFlashSFDP(struct target *target,uint8_t qspinum,uint32_t address){
    uint8_t data;
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.address = address;
    flash_msg.instruction = 0x5A;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 7;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 1;
    flash_msg.data_buffer = &data;
    QSPI_Transaction(target,qspinum,&flash_msg);
	return data;
}
/**
 * @fn uint32_t readNVCR(struct target *target,uint8_t qspinum, uint8_t* data)
 * 
 * @brief Used to read NVCR(Non volatile configuration register)
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param data The parameter \a data specifies pointer to location which has NVCR contents.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t readNVCR(struct target *target,uint8_t qspinum, uint8_t* data){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0xB5;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 2;
    flash_msg.data_buffer = data;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint32_t readJedecID(struct target *target,uint8_t qspinum, uint8_t *id)
 * 
 * @brief Used to write to NVCR(Non volatile configuration register)
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param id The parameter \a id specifies pointer which has pointer of variable which has JedecID.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t readJedecID(struct target *target,uint8_t qspinum, uint8_t *id){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0x9F;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_READ;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 3;
    flash_msg.data_buffer = id;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint32_t writeNVCR(struct target *target,uint8_t qspinum, uint8_t* data)
 * 
 * @brief Used to write to NVCR(Non volatile configuration register)
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param data The parameter \a data specifies pointer which has data to be written to NVCR.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t writeNVCR(struct target *target,uint8_t qspinum, uint8_t* data){
    flash_msg.address_mode = CCR_ADMODE_NIL;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.instruction = 0xB1;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_INDIRECT_WRITE;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 0;
    flash_msg.dummy_bit = 0;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 16;
    flash_msg.data_buffer = data;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
/**
 * @fn uint32_t flash_xip_init(struct target *target,uint8_t qspinum, int flash_size)
 * 
 * @brief Used to change QSPI mode to Excute In Place mode,used to run code from flash memory.
 * 
 * @param qspinum The parameter \a qspinum is an unsigned integer that represents the QSPI instance number.
 * @param flash_size The parameter \a flash_size specifies flash size.
 * 
 * @return SUCCESS if operation is successful and ENODEV if invalid instance number.
 */
uint32_t flash_xip_init(struct target *target,uint8_t qspinum, int flash_size){
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.FMEM_SIZE = flash_size;
    flash_msg.instruction = 0x6B;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_FOUR_LINE;
    flash_msg.functional_mode = CCR_FMODE_MMM;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 7;
    flash_msg.dummy_bit = 1;
    flash_msg.mm_mode = CCR_MM_MODE_XIP;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 0;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}

uint32_t psram_init(struct target *target,uint8_t qspinum, int ram_size,uint8_t fthresh){
    flash_msg.address_mode = CCR_ADMODE_SINGLE_LINE;
    flash_msg.address_size = CCR_ADSIZE_24_BIT;
    flash_msg.FMEM_SIZE = ram_size;
    flash_msg.instruction = 0x03;
    flash_msg.instruction_mode = CCR_IMODE_SINGLE_LINE;
    flash_msg.data_mode = CCR_DMODE_SINGLE_LINE;
    flash_msg.functional_mode = CCR_FMODE_MMM;
    flash_msg.dummy_mode = 0;
    flash_msg.dummy_cycles = 7;
    flash_msg.dummy_bit = 1;
    flash_msg.mm_mode = CCR_MM_MODE_RAM;
    flash_msg.alternate_byte_mode = CCR_ABMODE_NIL;
    flash_msg.length = 0;
    flash_msg.fthresh = fthresh;
    return QSPI_Transaction(target,qspinum,&flash_msg);
}
COMMAND_HANDLER(handle_flash_write)
{
/*
 * argv[1] = QSPI number
 * argv[2] = filename
 * argv[3] = size if code.bin is fed
 */
unsigned int qspi_number = 0;
//  uint32_t address = 0x30;
//  uint8_t data[16];
//  COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], qspi_number);
struct target *target __attribute__((unused)) = get_current_target(CMD_CTX);
FILE *file = fopen(CMD_ARGV[0], "rb");
if (file == NULL) {
    // perror("Error opening file");
    command_print(CMD, "File ");
    return -1;
}
uint32_t start_address;
Elf64_Ehdr elf_header;
// command_print(CMD, "Argc:%d\n", CMD_ARGC);
if(CMD_ARGC == 1){
// Read the ELF header

size_t bytesRead = fread(&elf_header, 1, sizeof(Elf64_Ehdr), file);
if (bytesRead != sizeof(Elf64_Ehdr)) {
    fclose(file);
    return -1;
}

// Check if the file is a valid ELF file by checking the magic number
if (elf_header.e_ident[EI_MAG0] != ELFMAG0 || 
    elf_header.e_ident[EI_MAG1] != ELFMAG1 || 
    elf_header.e_ident[EI_MAG2] != ELFMAG2 || 
    elf_header.e_ident[EI_MAG3] != ELFMAG3) {
    // fprintf(stderr, "This is not a valid ELF file\n");
    fclose(file);
    return -1;
}
   start_address = elf_header.e_entry;
}
else if(CMD_ARGC == 2){
   COMMAND_PARSE_NUMBER(uint, CMD_ARGV[1], start_address);
}
   // Print the entry point address (start address) in hexadecimal
command_print(CMD, "Start address: 0x%x\n", start_address);
if((start_address>=0x90000000) &&(start_address<=0xAFFFFFFF)){
   qspi_number = 0;
}else if((start_address>=0xB0000000) &&(start_address<=0xCFFFFFFF)){
   qspi_number = 1;
}
uint32_t mask_address =start_address&~(0xF<<28);
// command_print(CMD, "mask address: 0x%x\n", mask_address);
// command_print(CMD, "QSPI number: 0x%x\n", qspi_number);
// Variable to accumulate the total length of binary data for executable sections
size_t executable_binary_length = 0;
Elf64_Phdr phdr;
if(CMD_ARGC==1){
// Get the program header table offset and number of entries
fseek(file, elf_header.e_phoff, SEEK_SET);


for (int l = 0; l < elf_header.e_phnum; l++) {
    // Read the program header
    uint32_t val = fread(&phdr, sizeof(Elf64_Phdr), 1, file);
    if (val != 1) {
        fclose(file);
        return -1;  // Error reading program header
    }
    // Check if the current program header is of type PT_LOAD (executable segment)
    if (phdr.p_type == PT_LOAD) {
       executable_binary_length+=phdr.p_filesz;
    }
}
}
else if(CMD_ARGC==2){
       // Seek to the end to find the length of the file
   fseek(file, 0, SEEK_END);
   executable_binary_length  = ftell(file);
   rewind(file);  // Rewind to the beginning of the file
}
// Print the length of the executable binary data (excluding the ELF header)
command_print(CMD, "Length of executable binary data (excluding ELF header): %ld bytes\n", executable_binary_length);

/*handling sector erase operation*/
uint32_t progressed_length = 0;
log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "\nWriting code to flash in progress:\n");	
if(CMD_ARGC==1){
   
// Now read and process the program headers for executable content
FILE *fileheader = fopen(CMD_ARGV[0], "rb");
fseek(fileheader, elf_header.e_phoff, SEEK_SET);
for (int l = 0; l < elf_header.e_phnum; l++) {
   log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "PROGRAM HEADER:%x\n",l);	
    // Read the program header
    uint32_t val = fread(&phdr, sizeof(Elf64_Phdr), 1, fileheader);
    if (val != 1) {
        fclose(file);
        return -1;  // Error reading program header
    }
    // Check if the current program header is of type PT_LOAD (executable segment)
    if (phdr.p_type == PT_LOAD) {
        log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "Segment start virtual address: 0x%lx\n",phdr.p_paddr);
        fseek(file, phdr.p_offset, SEEK_SET); // Move to the segment's start
            unsigned char buffer[CHUNK_SIZE];
            uint8_t bytesReadInChunk;
            size_t remaining_bytes = phdr.p_filesz;
            log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "\nSection length:%lx\n",remaining_bytes);
            uint8_t to_read;
            mask_address=phdr.p_paddr&~(0xF<<28);
            uint32_t erase_start_address = mask_address & ~(0xFFF);
            uint32_t erase_end_address = (mask_address+remaining_bytes) & ~(0xFFF);
            for(uint32_t s = erase_start_address;s<=erase_end_address;s+=0x1000){
               log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "Erasing sector:%x\n",s);	
               writeEnable(target,qspi_number);/*Enable write operation*/
               sector4KErase(target,qspi_number,s);
               writeDisable(target,qspi_number);/*Enable write operation*/
            }
            size_t offset = 0x000;
            while (remaining_bytes > 0) {
               to_read = (remaining_bytes>CHUNK_SIZE)?CHUNK_SIZE:remaining_bytes;
               bytesReadInChunk = fread(buffer, 1, to_read, file);
               log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "\nWriting at offset:%lx\n",mask_address+offset);
               for(uint8_t i = 0;i<bytesReadInChunk;i++){
                   log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__,  "%x ",buffer[i]);
               }
               writeEnable(target,qspi_number);/*Enable write operation*/
               if(((mask_address+offset)&(~(0xFF))) == (((mask_address+offset)+bytesReadInChunk-1)&(~(0xFF)))){//check if start address and end address in same sector
                writeEnable(target,qspi_number);/*Enable write operation*/
                inputpageQuad(target,qspi_number,buffer,mask_address+offset,bytesReadInChunk);/*To write data to flash*/
                writeDisable(target,qspi_number);/*Enable write operation*/
            }
            else
            {
                log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "\nCrossing sector");
                uint32_t part1_address,part2_address;
                uint8_t part1_length,part2_length;
                part1_address = (mask_address+offset);
                part2_address = ((mask_address+offset)+bytesReadInChunk-1)&(~(0xFF));
                part1_length = (part2_address-(mask_address+offset));
                part2_length = (mask_address+offset)+bytesReadInChunk-part2_address;
                log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "\nCrossing sector part1 addr:%x",part1_address);
                log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "\nCrossing sector part2 addr:%x",part2_address);
                
                writeEnable(target,qspi_number);/*Enable write operation*/
                inputpageQuad(target,qspi_number,buffer,part1_address,part1_length);/*To write data to flash*/
                writeDisable(target,qspi_number);/*Enable write operation*/
                writeEnable(target,qspi_number);/*Enable write operation*/
                inputpageQuad(target,qspi_number,buffer+part1_length,part2_address,part2_length);/*To write data to flash*/
                writeDisable(target,qspi_number);/*Enable write operation*/
            }
                progressed_length +=bytesReadInChunk;
                print_progress_bar(progressed_length, executable_binary_length);
                offset += bytesReadInChunk;
                remaining_bytes-=bytesReadInChunk;
        }
    }
}
fclose(file);


}else if(CMD_ARGC==2){
   unsigned char buffer[CHUNK_SIZE];
   uint8_t bytesReadInChunk;
   size_t offset = 0x000;
   size_t remaining_bytes = executable_binary_length;
   uint8_t to_read;
   uint32_t erase_start_address = mask_address & ~(0xFFF);
   uint32_t erase_end_address = (mask_address+executable_binary_length) & ~(0xFFF);
   for(uint32_t s = erase_start_address;s<=erase_end_address;s+=0x1000){
   log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "Erasing sector:%x\n",s);	
   writeEnable(target,qspi_number);/*Enable write operation*/
   sector4KErase(target,qspi_number,s);
   writeDisable(target,qspi_number);/*Enable write operation*/
   }
   while (remaining_bytes > 0) {
      to_read = (remaining_bytes>CHUNK_SIZE)?CHUNK_SIZE:remaining_bytes;
      bytesReadInChunk = fread(buffer, 1, to_read, file);
      writeEnable(target,qspi_number);/*Enable write operation*/
      inputpageQuad(target,qspi_number,buffer,mask_address+offset,bytesReadInChunk);/*To write data to flash*/
      writeDisable(target,qspi_number);/*Enable write operation*/
      log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__, "\nWriting at offset:%lx\n",start_address+offset);
      for(uint8_t i = 0;i<16;i++){
          log_printf(LOG_LVL_DEBUG, __FILE__, __LINE__, __func__,  "%x ",buffer[i]);
      }
      progressed_length +=bytesReadInChunk;
      print_progress_bar(progressed_length, executable_binary_length);
       offset += bytesReadInChunk;
       remaining_bytes-=bytesReadInChunk;
}
// Close the file
fclose(file);
}
log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "\nCompleted writing!!");	
command_print(CMD, "Completed writing");
return ERROR_OK;
}



COMMAND_HANDLER(handle_flash_write_length)
{
/*
 * argv[1] = QSPI number
 * argv[2] = filename
 * argv[3] = size if code.bin is fed
 */
uint8_t flag=0;
unsigned int qspi_number = 0;
//  uint32_t address = 0x30;
//  uint8_t data[16];
//  COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], qspi_number);
struct target *target __attribute__((unused)) = get_current_target(CMD_CTX);
const char *ext = strrchr(CMD_ARGV[0], '.');
    // Check if an extension exists
    if (ext != NULL) {
        // Compare the extension with the desired formats
        if (strcmp(ext, ".bin") == 0) {
            flag = 2;
        } else if (strcmp(ext, ".elf") == 0 || strcmp(ext, ".shakti") == 0) {
            flag = 1;
        } else {
            command_print(CMD,"Invalid file format\n");
        }
    } else {
        command_print(CMD,"No extension found\n");
    }

FILE *file = fopen(CMD_ARGV[0], "rb");
if (file == NULL) {
    // perror("Error opening file");
    command_print(CMD, "File doesnt exist");
    return -1;
}
Elf64_Ehdr elf_header;
if(flag==1){
    size_t bytesRead = fread(&elf_header, 1, sizeof(Elf64_Ehdr), file);
if (bytesRead != sizeof(Elf64_Ehdr)) {
    fclose(file);
    return -1;
}
}


uint32_t start_address;

// Read the ELF header
COMMAND_PARSE_NUMBER(uint, CMD_ARGV[1], start_address);
// Print the entry point address (start address) in hexadecimal
command_print(CMD, "(start address): 0x%x\n", start_address);
if((start_address>=0x90000000) &&(start_address<=0xAFFFFFFF)){
   qspi_number = 0;
}else if((start_address>=0xB0000000) &&(start_address<=0xCFFFFFFF)){
   qspi_number = 1;
}
uint32_t mask_address =start_address&~(0xF<<28);
command_print(CMD, "mask address: 0x%x\n", mask_address);
command_print(CMD, "QSPI number: 0x%x\n", qspi_number);
// Variable to accumulate the total length of binary data for executable sections
size_t executable_binary_length = 0;
Elf64_Phdr phdr;
if(flag==1){
// Get the program header table offset and number of entries
fseek(file, elf_header.e_phoff, SEEK_SET);


for (int l = 0; l < elf_header.e_phnum; l++) {
    // Read the program header
    uint32_t val = fread(&phdr, sizeof(Elf64_Phdr), 1, file);
    if (val != 1) {
        fclose(file);
        return -1;  // Error reading program header
    }
    // Check if the current program header is of type PT_LOAD (executable segment)
    if (phdr.p_type == PT_LOAD) {
       executable_binary_length+=phdr.p_filesz;
    }
}
}
else if(flag==2){
       // Seek to the end to find the length of the file
   fseek(file, 0, SEEK_END);
   executable_binary_length  = ftell(file);
   rewind(file);  // Rewind to the beginning of the file
}
uint64_t executable_binary_length_copy = (uint64_t)executable_binary_length;
uint8_t *ptr;     
ptr =(uint8_t*)&executable_binary_length_copy;
// writeEnable(target,qspi_number);/*Enable write operation*/
// sector4KErase(target,qspi_number,mask_address & ~(0xFFF));
// writeDisable(target,qspi_number);/*Enable write operation*/
writeEnable(target,qspi_number);/*Enable write operation*/
inputpageQuad(target,qspi_number,ptr,mask_address,8);/*To write data to flash*/
writeDisable(target,qspi_number);/*Enable write operation*/
// Print the length of the executable binary data (excluding the ELF header)
command_print(CMD, "Length of executable binary data (excluding ELF header): 0x%lx bytes\n", executable_binary_length_copy);
command_print(CMD, "Completed writing length at mask address :%x",mask_address);
return ERROR_OK;
}


COMMAND_HANDLER(handle_flash_write_data)
{
/*
 * argv[1] = address
 * argv[2 ... n-1] = data
 */
    uint32_t start_address,length=0;
    uint32_t total_length = (CMD_ARGC)-1;
    uint8_t qspi_number=0;
    uint8_t __attribute__((unused)) data[CMD_ARGC-1];
    struct target *target __attribute__((unused)) = get_current_target(CMD_CTX);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], start_address);
    if((start_address>=0x90000000) &&(start_address<=0xAFFFFFFF)){
        qspi_number = 0;
    }else if((start_address>=0xB0000000) &&(start_address<=0xCFFFFFFF)){
        qspi_number = 1;
    }
    // printf("Start address :%x",start_address);
    // log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__,  "\nqspi_number :%x ,total_length :%x",qspi_number,total_length);
    // Read the ELF header
    for(uint32_t i = 0;i<total_length;i++){
        COMMAND_PARSE_NUMBER(u8, CMD_ARGV[i+1], data[i]);
    }
    // for(uint8_t j = 0;j<total_length;j++){
    //       log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__,  "%x ",data[j]);
    // }
    uint8_t *ptr = data;
    for(uint32_t address __attribute__((unused)) = start_address&~(0xF<<28),l=0,remaining_length=total_length;remaining_length;address+=length){
        length = (remaining_length>16)?16:remaining_length;
            writeEnable(target,qspi_number);/*Enable write operation*/
            inputpageQuad(target,qspi_number,ptr+l,address,length);/*To write data to flash*/
            writeDisable(target,qspi_number);/*Enable write operation*/
        //     printf("Every iteration data: %x\n",address);
        //     log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__,  "Address : %x\n",address);
        //     for(uint8_t k = 0;k<length;k++){
        //     log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__,  "%x ",(ptr+l)[k]);
        // }
        remaining_length-=length;
        l+=length;
    }
return ERROR_OK;
}














COMMAND_HANDLER(handle_sector_erase)
{
/*
 * argv[1] = QSPI number
 * 
 */
unsigned int start_address,no_of_sectors,mode,qspi_number=0;
COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], mode);
COMMAND_PARSE_NUMBER(uint, CMD_ARGV[1], start_address);
COMMAND_PARSE_NUMBER(uint, CMD_ARGV[2], no_of_sectors);
struct target *target = get_current_target(CMD_CTX);
command_print(CMD, "Requested sectors are erased");
log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "Sector Erase in Progress!!");
if((start_address>=0x90000000) &&(start_address<=0xAFFFFFFF)){
   qspi_number = 0;
}else if((start_address>=0xB0000000) &&(start_address<=0xCFFFFFFF)){
   qspi_number = 1;
}
uint32_t mask_value =(mode==4)?(~(0xFFF)):((mode==32)?~(0x7FFF):0);
uint32_t increment=(mode==4)?(0x1000):((mode==32)?(0x8000):0);
uint32_t mask_address =start_address&~(0xF<<28);
uint32_t erase_start_address = mask_address & mask_value;
for(uint32_t s = erase_start_address,i=0;i<no_of_sectors;s+=increment,i++){
   log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "Erasing sector:%x\n",s);
   writeEnable(target,qspi_number);/*Enable write operation*/
   if(mode==4)
   sector4KErase(target,qspi_number,s);
   else if (mode==32)
   sector32KErase(target,qspi_number,s);
   writeDisable(target,qspi_number);/*Enable write operation*/
}
return 0;
}

COMMAND_HANDLER(handle_reset)
{
struct target *target = get_current_target(CMD_CTX);
target_write_u32(target,0x40408,3);
target_write_u32(target,0x40400,0);
return 0;
}


COMMAND_HANDLER(handle_flash_erase)
{
/*
 * argv[1] = QSPI number
 * 
 */
unsigned int qspi_number;
uint8_t sr;
COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], qspi_number);
struct target *target = get_current_target(CMD_CTX);
command_print(CMD, "Flash erase is invoked with qspi %x",qspi_number);
command_print(CMD, "Wait Chip Erase in Progress!!");
log_printf(LOG_LVL_OUTPUT, __FILE__, __LINE__, __func__, "Wait Chip Erase in Progress!!");
writeEnable(target,qspi_number);/*Enable write operation*/
chipErase(target,qspi_number);/*Send chip erase command*/
writeDisable(target,qspi_number);/*Disable write operation*/
uint8_t temp = 0;
while(1)
{
temp =  readFlagStatusRegister(target,qspi_number);
sr = readStatusRegister1(target,qspi_number);
temp = temp & (1<<7);
if(temp == (1<<7)&&(sr == 0)){/*Wait till ready bit is set,used to check if erase operation is in progress*/
    break; 
}
}
command_print(CMD, "Chip Erase Complete!"); 
return ERROR_OK;
}

COMMAND_HANDLER(handle_flash_xip)
{
/*
 * argv[1] = QSPI number
 * 
 */
unsigned int qspi_number;
COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], qspi_number);
struct target *target = get_current_target(CMD_CTX);
command_print(CMD, "Flash xip is configured with qspi %x",qspi_number);
flash_xip_init(target,qspi_number,27);
return ERROR_OK;
}
static const struct command_registration secureiot_exec_command_handlers[] = {
    {
        .name = "flash_erase",
        .mode = COMMAND_EXEC,
        .handler = handle_flash_erase,
        .help = "Flash erase command",
        .usage = "fec",
    },
    {
       .name = "flash_write",
       .mode = COMMAND_EXEC,
       .handler = handle_flash_write,
       .help = "Flash write command",
       .usage = "fwc",
   },
    {
       .name = "flash_xip_init",
       .mode = COMMAND_EXEC,
       .handler = handle_flash_xip,
       .help = "Flash xip command",
       .usage = "fxc",
   },
   {
       .name = "flash_sector_erase",
       .mode = COMMAND_EXEC,
       .handler = handle_sector_erase,
       .help = "Flash xip command",
       .usage = "fsec",
   },
   {
       .name = "reset",
       .mode = COMMAND_EXEC,
       .handler = handle_reset,
       .help = "reset command",
       .usage = "fsec",
   },
   {
        .name = "flash_write_length",
        .mode = COMMAND_EXEC,
        .handler = handle_flash_write_length,
        .help = "reset command",
        .usage = "fsec",
   },    
      {
        .name = "flash_write_data",
        .mode = COMMAND_EXEC,
        .handler = handle_flash_write_data,
        .help = "reset command",
        .usage = "fsec",
   },    
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration secureiot_command_handlers[] = {
	{
		.name = "secureiot",
		.mode = COMMAND_ANY,
		.help = "secureiot flash command group",
		.usage = "Summa print",
		.chain = secureiot_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};
FLASH_BANK_COMMAND_HANDLER(secureiot_flash_bank_command)
{


	return ERROR_OK;
}


static int secure_iot_erase(struct flash_bank *bank, unsigned int first,
    unsigned int last)
{
    // Perform erase operation and return ERROR_OK.
    return ERROR_OK;
}

static int secure_iot_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
    // Perform protect operation and return ERROR_OK.
    return ERROR_OK;
}

static int secure_iot_write(struct flash_bank *bank, const uint8_t *buffer,
	uint32_t offset, uint32_t count)
{
    // Perform write operation and return ERROR_OK.
    return ERROR_OK;
}


static int secure_iot_probe(struct flash_bank *bank)
{
    // Perform probe operation and return ERROR_OK.
    return ERROR_OK;
}

static int secure_iot_auto_probe(struct flash_bank *bank)
{
    // Perform auto probe operation and return ERROR_OK.
    return ERROR_OK;
}


static int secure_iot_protect_check(struct flash_bank *bank)
{
    // Perform protect check operation and return ERROR_OK.
    return ERROR_OK;
}

static int get_secure_iot_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    // Retrieve secure_iot information and return ERROR_OK.
    return ERROR_OK;
}
const struct flash_driver secureiot_flash = {
	.name = "secureiot",
	.commands = secureiot_command_handlers,
	.flash_bank_command = secureiot_flash_bank_command,
	.erase = secure_iot_erase,
	.protect = secure_iot_protect,
	.write = secure_iot_write,
	.read = default_flash_read,
	.probe = secure_iot_probe,
	.auto_probe = secure_iot_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = secure_iot_protect_check,
	.info = get_secure_iot_info,
	.free_driver_priv = default_flash_free_driver_priv,
};
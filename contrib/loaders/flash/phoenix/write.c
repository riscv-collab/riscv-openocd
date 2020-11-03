#define FLASH_BASE (0x10100000UL)   /*!< ( FLASH   ) Base Address */
#define NVR_BASE (0x10140000UL)     /*!< ( NVR     ) Base Address */
#define EEPROM_BASE (0x10180000UL)  /*!< ( EEPROM  ) Base Address */
#define PAGEBUF_BASE (0x101C0000UL) /*!< ( PAGEBUF ) Base Address */
#define EFC_BASE (0x40000000UL)

#define EFC_CR (EFC_BASE + 0x00)
#define EFC_Tnvs (EFC_BASE + 0x04)
#define EFC_Tprog (EFC_BASE + 0x08)
#define EFC_Tpgs (EFC_BASE + 0x0C)
#define EFC_Trcv (EFC_BASE + 0x10)
#define EFC_Terase (EFC_BASE + 0x14)
#define EFC_WPT (EFC_BASE + 0x18)
#define EFC_OPR (EFC_BASE + 0x1C)
#define EFC_PVEV (EFC_BASE + 0x20)
#define EFC_STS (EFC_BASE + 0x24)

#define REG(x) (*(int *)(x))

#define EFC_OP(op, offset)              \
    {                                   \
        REG(EFC_STS) = 0xff;            \
        REG(EFC_OPR) = (op);            \
        REG(EFC_OPR) = 0x70 + (op);     \
        REG(EFC_OPR) = 0x90 + (op);     \
        REG(EFC_OPR) = 0xC0 + (op);     \
        REG(FLASH_BASE + (offset)) = 1; \
    }

int start(int offset, int buffer, int len)
{
    while (len > 0)
    {
        EFC_OP(2, offset);
        if (REG(EFC_STS) != 1)
            break;
        for (int i = 0; i < 512 && i < len; i += 4)
        {
            REG(PAGEBUF_BASE + i) = REG(buffer + i);
        }
        EFC_OP(1, offset);
        if (REG(EFC_STS) != 1)
            break;
        EFC_OP(1, offset + 256);
        if (REG(EFC_STS) != 1)
            break;
        offset += 512;
        buffer += 512;
        len -= 512;
    }

    return REG(EFC_STS);
}
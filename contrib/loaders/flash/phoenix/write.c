#define FLASH_BASE (0x10100000UL)   /*!< ( FLASH   ) Base Address */
#define PAGEBUF_BASE (0x101C0000UL) /*!< ( PAGEBUF ) Base Address */
#define MODEL_CHK (0x40001020UL)
#define EFC_OPR (0x4000001CUL)
#define EFC_STS (0x40000024UL)

#define REG32(addr) (*((volatile unsigned int *)(addr)))

int stack[32] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

int start(int offset, int buffer, int len)
{
    REG32(MODEL_CHK) = 0x05;
    int model = REG32(MODEL_CHK);
    while (len > 0)
    {
        // 擦除本页
        REG32(EFC_STS) = 0xFF;
        REG32(EFC_OPR) = 0x02;
        REG32(EFC_OPR) = 0x72;
        REG32(EFC_OPR) = 0x92;
        REG32(EFC_OPR) = 0xC2;
        REG32(FLASH_BASE + offset) = 1;

        if (REG32(EFC_STS) != 1)
            break;
        if (model == 0x05) // S301
        {
            for (int i = 0; i < 512 && i < len; i += 4)
            {
                REG32(PAGEBUF_BASE + i) = REG32(buffer + i);
            }
            // 行编程
            REG32(EFC_OPR) = 0x01;
            REG32(EFC_OPR) = 0x71;
            REG32(EFC_OPR) = 0x91;
            REG32(EFC_OPR) = 0xC1;
            REG32(EFC_STS) = 0xFF;
            REG32(FLASH_BASE + offset) = 1;
            if (REG32(EFC_STS) != 1)
                break;
            // 行编程, 第二个256字节
            REG32(EFC_OPR) = 0x01;
            REG32(EFC_OPR) = 0x71;
            REG32(EFC_OPR) = 0x91;
            REG32(EFC_OPR) = 0xC1;
            REG32(EFC_STS) = 0xFF;
            REG32(FLASH_BASE + offset + 256) = 1;
            if (REG32(EFC_STS) != 1)
                break;
        }
        else if (model == 0x00) // S302
        {
            // 关闭编程保护
            REG32(EFC_OPR) = 0x00;
            REG32(EFC_OPR) = 0x70;
            REG32(EFC_OPR) = 0x90;
            REG32(EFC_OPR) = 0xC0;
            for (int i = 0; i < 512 && i < len; i += 4)
            {
                // 单字编程
                REG32(EFC_STS) = 0xFF;
                REG32(FLASH_BASE + offset + i) = REG32(buffer + i);
                for (int j = 0; j < 10000; j++)
                {
                    if (REG32(EFC_STS) == 1)
                        break;
                }
                if (REG32(EFC_STS) != 1)
                    break;
            }
            // 开启编程保护
            REG32(EFC_OPR) = 0xFF;
        }
        else // 不能识别
        {
            return 0;
        }

        offset += 512;
        buffer += 512;
        len -= 512;
    }

    return REG32(EFC_STS);
}
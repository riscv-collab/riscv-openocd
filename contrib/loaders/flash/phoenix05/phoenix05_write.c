#define FLASH_BASE (0x00002000UL)   /*!< ( FLASH   ) Base Address */
#define MODEL_CHK  (0x0000C3FCUL)
#define EFC_OPR    (0x0000C01CUL)
#define EFC_STS    (0x0000C024UL)

#define REG32(addr) (*((volatile unsigned int *)(addr)))

int stack[32] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

int start(int offset, int buffer, int len)
{
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
        if (model == 0xF05) // S305
        {
            // 关闭编程保护
            REG32(EFC_OPR) = 0x00;
            REG32(EFC_OPR) = 0x70;
            REG32(EFC_OPR) = 0x90;
            REG32(EFC_OPR) = 0xC0;
            for (int i = 0; i < 128 && i < len; i += 4)
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

        offset += 128;
        buffer += 128;
        len -= 128;
    }

    return REG32(EFC_STS);
}

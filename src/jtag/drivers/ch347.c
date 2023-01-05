/***************************************************************************                                                                     *
 *   Driver for CH347-JTAG interface V1.0                                  *
 *                                                                         *
 *   Copyright (C) 2022 Nanjing Qinheng Microelectronics Co., Ltd.         *
 *   Web: http://wch.cn                                                    *
 *   Author: WCH@TECH53 <tech@wch.cn>                                      *
 *                                                                         *
 *   CH347 is a high-speed USB bus converter chip that provides UART, I2C  *
 *   and SPI synchronous serial ports and JTAG interface through USB bus.  * 
 *                                                                         *
 *   The USB2.0 to JTAG scheme based on CH347 can be used to build         *
 *   customized USB high-speed JTAG debugger and other products.           *                                                          
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if IS_CYGWIN == 1
#include "windows.h"
#undef LOG_ERROR
#endif

/* project specific includes */
#include <jtag/interface.h>
#include <jtag/commands.h>
#include <helper/time_support.h>
#include <helper/replacements.h>

/* system includes */
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define JTAGIO_STA_OUT_TDI (0x10)
#define JTAGIO_STA_OUT_TMS (0x02)
#define JTAGIO_STA_OUT_TCK (0x01)

#define TDI_H JTAGIO_STA_OUT_TDI
#define TDI_L 0
#define TMS_H JTAGIO_STA_OUT_TMS
#define TMS_L 0
#define TCK_H JTAGIO_STA_OUT_TCK
#define TCK_L 0

#define KHZ(n) ((n)*UINT64_C(1000))
#define MHZ(n) ((n)*UINT64_C(1000000))
#define GHZ(n) ((n)*UINT64_C(1000000000))

#define HW_TDO_BUF_SIZE              4096
#define SF_PACKET_BUF_SIZE           51200 //命令包长度
#define UCMDPKT_DATA_MAX_BYTES_USBHS 507   // USB高速时每个命令包内包含数据长度
#define USBC_PACKET_USBHS            512   // USB高速时单包最大数据长度

#define CH347_CMD_HEADER 3 //协议包头长度

// 协议传输格式：CMD（1字节）+ Length（2字节）+ Data
#define CH347_CMD_INFO_RD            0xCA //参数获取,用于获取固件版本、JTAG接口相关参数等
#define CH347_CMD_JTAG_INIT          0xD0 // JTAG接口初始化命令
#define CH347_CMD_JTAG_BIT_OP        0xD1 // JTAG接口引脚位控制命令
#define CH347_CMD_JTAG_BIT_OP_RD     0xD2 // JTAG接口引脚位控制并读取命令
#define CH347_CMD_JTAG_DATA_SHIFT    0xD3 // JTAG接口数据移位命令
#define CH347_CMD_JTAG_DATA_SHIFT_RD 0xD4 // JTAG接口数据移位并读取命令

#pragma pack(1)

typedef struct _CH347_info { // 记录CH347引脚状态
    int TMS;
    int TDI;
    int TCK;

    int buffer_idx;
    uint8_t buffer[HW_TDO_BUF_SIZE];
} _CH347_Info;

#pragma pack()

#ifdef _WIN32
#include <windows.h>
typedef int(__stdcall *pCH347OpenDevice)(unsigned long iIndex);
typedef void(__stdcall *pCH347CloseDevice)(unsigned long iIndex);
typedef unsigned long(__stdcall *pCH347SetTimeout)(unsigned long iIndex,        // 指定设备序号
                                              unsigned long iWriteTimeout, // 指定USB写出数据块的超时时间,以毫秒mS为单位,0xFFFFFFFF指定不超时(默认值)
                                              unsigned long iReadTimeout); // 指定USB读取数据块的超时时间,以毫秒mS为单位,0xFFFFFFFF指定不超时(默认值)
typedef unsigned long(__stdcall *pCH347WriteData)(unsigned long iIndex,         // 指定设备序号
                                                  void *oBuffer,                // 指向一个足够大的缓冲区,用于保存描述符
                                                  unsigned long *ioLength);     // 指向长度单元,输入时为准备读取的长度,返回后为实际读取的长度
typedef unsigned long(__stdcall *pCH347ReadData)(unsigned long iIndex,          // 指定设备序号
                                                 void *oBuffer,                 // 指向一个足够大的缓冲区,用于保存描述符
                                                 unsigned long *ioLength);      // 指向长度单元,输入时为准备读取的长度,返回后为实际读取的长度
typedef unsigned long(__stdcall *pCH347Jtag_INIT)(unsigned long iIndex,         // 指定设备序号
                                                  unsigned char iClockRate);    // 指向长度单元,输入时为准备读取的长度,返回后为实际读取的长度
HMODULE uhModule;
BOOL ugOpen;
unsigned long ugIndex;
pCH347OpenDevice CH347OpenDevice;
pCH347CloseDevice CH347CloseDevice;
pCH347SetTimeout CH347SetTimeout;
pCH347ReadData CH347ReadData;
pCH347WriteData CH347WriteData;
pCH347Jtag_INIT CH347Jtag_INIT;
#elif defined(__linux__) 
#include <CH347LIB.h>
bool ugOpen;
unsigned long ugIndex = 0;
#endif 

int DevIsOpened; // 设备是否打开
bool UsbHighDev = true;
unsigned long USBC_PACKET;

_CH347_Info ch347 = {0, 0, 0, 0, ""}; // 初始化设备结构状态

/**
 *  HexToString - Hex转换字符串函数
 *  @param buf    指向一个缓冲区,放置准备转换的Hex数据
 *  @param size   指向需要转换数据的长度单元
 *
 *  @return 	  返回转换后字符串
 */
static char *HexToString(uint8_t *buf, uint32_t size)
{
    uint32_t i;
    char *str = calloc(size * 2 + 1, 1);

    for (i = 0; i < size; i++)
        sprintf(str + 2 * i, "%02x ", buf[i]);
    return str;
}

/**
 *  CH347_Write - CH347 写方法
 *  @param oBuffer    指向一个缓冲区,放置准备写出的数据
 *  @param ioLength   指向长度单元,输入时为准备写出的长度,返回后为实际写出的长度
 *
 *  @return 		  写成功返回1，失败返回0
 */
static int CH347_Write(void *oBuffer, unsigned long *ioLength)
{
    int ret = -1;
    unsigned long wlength = *ioLength, WI;

    if (*ioLength >= HW_TDO_BUF_SIZE)
        wlength = HW_TDO_BUF_SIZE;
    WI = 0;
    while (1) {
        ret = CH347WriteData(ugIndex, oBuffer + WI, &wlength);        
        LOG_DEBUG_IO("(size=%lu, buf=[%s]) -> %" PRIu32, wlength, HexToString((uint8_t *)oBuffer, wlength), (uint32_t)wlength);
        WI += wlength;
        if (WI >= *ioLength)
            break;
        if ((*ioLength - WI) > HW_TDO_BUF_SIZE)
            wlength = HW_TDO_BUF_SIZE;
        else
            wlength = *ioLength - WI;
    }

    *ioLength = WI;
    return ret;
}

/**
 * CH347_Read - CH347 读方法
 * @param oBuffer  	指向一个足够大的缓冲区,用于保存读取的数据
 * @param ioLength 	指向长度单元,输入时为准备读取的长度,返回后为实际读取的长度
 *
 * @return 			读成功返回1，失败返回0
 */
static int CH347_Read(void *oBuffer, unsigned long *ioLength)
{
    unsigned long rlength = *ioLength;
    // 单次读取最大允许读取4096B数据，超过则按4096B进行计算
    if (rlength > HW_TDO_BUF_SIZE)
        rlength = HW_TDO_BUF_SIZE;
    
    if (!CH347ReadData(ugIndex, oBuffer, &rlength)) 
    {
        LOG_ERROR("CH347_Read read data failure.");
        return false;
    }

    LOG_DEBUG_IO("(size=%lu, buf=[%s]) -> %" PRIu32, rlength, HexToString((uint8_t *)oBuffer, rlength), (uint32_t)rlength);
    *ioLength = rlength;
    return true;
}

/**
 * CH347_ClockTms - 功能函数，用于在TCK的上升沿改变TMS值，使其Tap状态切换
 * @param BitBangPkt 	协议包
 * @param tms 		 	需要改变的TMS值
 * @param BI		 	协议包长度
 *
 * @return 			 	返回协议包长度
 */
static unsigned long CH347_ClockTms(unsigned char *BitBangPkt, int tms, unsigned long BI)
{
    unsigned char cmd = 0;

    if (tms == 1)
        cmd = TMS_H;
    else
        cmd = TMS_L;

    BitBangPkt[BI++] = cmd | TDI_H | TCK_L;
    BitBangPkt[BI++] = cmd | TDI_H | TCK_H;

    ch347.TMS = cmd;
    ch347.TDI = TDI_H;
    ch347.TCK = TCK_H;

    return BI;
}

/**
 * CH347_IdleClock - 功能函数，确保时钟处于拉低状态
 * @param BitBangPkt 	 协议包
 * @param BI  		 	 协议包长度
 *
 * @return 			 	 返回协议包长度
 */
static unsigned long CH347_IdleClock(unsigned char *BitBangPkt, unsigned long BI)
{
    unsigned char byte = 0;
    byte |= ch347.TMS ? TMS_H : TMS_L;
    byte |= ch347.TDI ? TDI_H : TDI_L;
    BitBangPkt[BI++] = byte;

    return BI;
}

/**
 * CH347_TmsChange - 功能函数，通过改变TMS的值来进行状态切换
 * @param tmsValue 		 需要进行切换的TMS值按切换顺序组成一字节数据
 * @param step 	   		 需要读取tmsValue值的位值数
 * @param skip 	   		 从tmsValue的skip位处开始计数到step
 *
 */
static void CH347_TmsChange(const unsigned char *tmsValue, int step, int skip)
{
    int i;
    unsigned long BI, retlen, TxLen;
    unsigned char BitBangPkt[4096] = "";

    BI = CH347_CMD_HEADER;
    retlen = CH347_CMD_HEADER;
    LOG_DEBUG_IO("(TMS Value: %02x..., step = %d, skip = %d)", tmsValue[0], step, skip);

    for (i = skip; i < step; i++) {
        retlen = CH347_ClockTms(BitBangPkt, (tmsValue[i / 8] >> (i % 8)) & 0x01, BI);
        BI = retlen;
    }
    BI = CH347_IdleClock(BitBangPkt, BI);
    BitBangPkt[0] = CH347_CMD_JTAG_BIT_OP;
    BitBangPkt[1] = (unsigned char)BI - CH347_CMD_HEADER;
    BitBangPkt[2] = 0;

    TxLen = BI;

    if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
        LOG_ERROR("JTAG Write send usb data failure.");
        return;
    }
}

/**
 * CH347_TMS - 由ch347_execute_queue调用
 * @param cmd 	   上层传递命令参数
 *
 */
static void CH347_TMS(struct tms_command *cmd)
{
    LOG_DEBUG_IO("(step: %d)", cmd->num_bits);
    CH347_TmsChange(cmd->bits, cmd->num_bits, 0);
}

/**
 * CH347_Reset - CH347 复位Tap状态函数
 * @brief 	连续六个以上TCK且TMS为高将可将状态机置为Test-Logic Reset状态
 *
 */
static int CH347_Reset(void)
{
    unsigned char BitBang[512] = "", BI, i;
    unsigned long TxLen;

    BI = CH347_CMD_HEADER;
    for (i = 0; i < 7; i++) {
        BitBang[BI++] = TMS_H | TDI_H | TCK_L;
        BitBang[BI++] = TMS_H | TDI_H | TCK_H;
    }
    BitBang[BI++] = TMS_H | TDI_H | TCK_L;

    BitBang[0] = CH347_CMD_JTAG_BIT_OP;
    BitBang[1] = BI - CH347_CMD_HEADER;
    BitBang[2] = 0;

    TxLen = BI;

    if (!CH347_Write(BitBang, &TxLen) && (TxLen != BI)) {
        LOG_ERROR("JTAG_Init send usb data failure.");
        return false;
    }
    return true;
}

/**
 * CH347_MovePath - 获取当前Tap状态并切换至cmd传递下去的状态TMS值
 * @param cmd 上层传递命令参数
 *
 */
static void CH347_MovePath(struct pathmove_command *cmd)
{
    int i;
    unsigned long BI, retlen = 0, TxLen;
    unsigned char BitBangPkt[4096] = "";

    BI = CH347_CMD_HEADER;

    LOG_DEBUG_IO("(num_states=%d, last_state=%d)",
                 cmd->num_states, cmd->path[cmd->num_states - 1]);

    for (i = 0; i < cmd->num_states; i++) {
        if (tap_state_transition(tap_get_state(), false) == cmd->path[i])
            retlen = CH347_ClockTms(BitBangPkt, 0, BI);
        BI = retlen;
        if (tap_state_transition(tap_get_state(), true) == cmd->path[i])
            retlen = CH347_ClockTms(BitBangPkt, 1, BI);
        BI = retlen;
        tap_set_state(cmd->path[i]);
    }

    BI = CH347_IdleClock(BitBangPkt, BI);
    BitBangPkt[0] = CH347_CMD_JTAG_BIT_OP;
    BitBangPkt[1] = (unsigned char)BI - CH347_CMD_HEADER;
    BitBangPkt[2] = 0;

    TxLen = BI;
    if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
        LOG_ERROR("JTAG Write send usb data failure.");
        return;
    }
}

/**
 * CH347_MoveState - 切换Tap状态至目标状态stat
 * @param stat 预切换目标路径
 * @param skip 需跳过的位数
 *
 */
static void CH347_MoveState(tap_state_t state, int skip)
{
    uint8_t tms_scan;
    int tms_len;

    LOG_DEBUG_IO("(from %s to %s)", tap_state_name(tap_get_state()),
                 tap_state_name(state));
    if (tap_get_state() == state)
        return;
    tms_scan = tap_get_tms_path(tap_get_state(), state);
    tms_len = tap_get_tms_path_len(tap_get_state(), state);
    CH347_TmsChange(&tms_scan, tms_len, skip);
    tap_set_state(state);
}

/**
 * CH347_WriteRead - CH347 批量读写函数
 * @param bits 			 此次进行读写数据
 * @param nb_bits 		 传入数据长度
 * @param scan			 传入数据的传输方式来确定是否执行数据读取
 *
 */
static void CH347_WriteRead(uint8_t *bits, int nb_bits, enum scan_type scan)
{
    // uint32_t delay = 1000000;
    int nb8 = nb_bits / 8;
    int nb1 = nb_bits % 8;
    int i;
    bool IsRead = false;
    uint8_t TMS_Bit, TDI_Bit = 0;
    uint8_t *tdos = calloc(1, nb_bits / 8 + 32);
    static uint8_t BitBangPkt[SF_PACKET_BUF_SIZE];
    static uint8_t byte0[SF_PACKET_BUF_SIZE];
    unsigned char temp[512] = "";
    unsigned char temp_a[512] = "";
    unsigned long BI = 0, TxLen, RxLen, DI, DII, PktDataLen, DLen;
    int ret = ERROR_OK;

    // 最后一个TDI位将会按照位带模式输出，其nb1确保不为0，使其能在TMS变化时输出最后1bit数据
    if (nb8 > 0 && nb1 == 0) {
        nb8--;
        nb1 = 8;
    }

    IsRead = (scan == SCAN_IN || scan == SCAN_IO);
    DI = BI = 0;
    while (DI < (unsigned long)nb8) {
        // 构建数据包
        if ((nb8 - DI) > UCMDPKT_DATA_MAX_BYTES_USBHS)
            PktDataLen = UCMDPKT_DATA_MAX_BYTES_USBHS;
        else
            PktDataLen = nb8 - DI;

        DII = PktDataLen;

        if (IsRead)
            BitBangPkt[BI++] = CH347_CMD_JTAG_DATA_SHIFT_RD;
        else
            BitBangPkt[BI++] = CH347_CMD_JTAG_DATA_SHIFT;

        BitBangPkt[BI++] = (uint8_t)(PktDataLen >> 0) & 0xFF;
        BitBangPkt[BI++] = (uint8_t)(PktDataLen >> 8) & 0xFF;

        if (bits)
            memcpy(&BitBangPkt[BI], &bits[DI], PktDataLen);
        else
            memcpy(&BitBangPkt[BI], byte0, PktDataLen);
        BI += PktDataLen;

        // 若需回读数据则判断当前BI值进行命令下发
        if (IsRead) {
            TxLen = BI;

            if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
                LOG_ERROR("CH347_WriteRead write usb data failure.");
                return;
            }
            BI = 0;

            ret = ERROR_OK;
            while (ret == ERROR_OK && PktDataLen > 0) {
                RxLen = PktDataLen + CH347_CMD_HEADER;
                if (!(ret = CH347_Read(temp, &RxLen))) {
                    LOG_ERROR("CH347_WriteRead read usb data failure.\n");
                    return;
                }

                if (RxLen != TxLen) {
                    if (!(ret = CH347_Read(temp_a, &TxLen))) {
                        LOG_ERROR("CH347_WriteRead read usb data failure.\n");
                        return;
                    }
                    memcpy(&temp[RxLen], temp_a, TxLen);
                    RxLen += TxLen;
                }

                if (RxLen != 0)
                    memcpy(&tdos[DI], &temp[CH347_CMD_HEADER], (RxLen - CH347_CMD_HEADER));
                PktDataLen -= RxLen;
            }
        }

        DI += DII;

        // 在传输过程中，若不回读则根据命令包长度将要达到饱和时将命令下发
        if (((SF_PACKET_BUF_SIZE - BI) < USBC_PACKET || (SF_PACKET_BUF_SIZE - BI) == USBC_PACKET)) {
            TxLen = BI;

            if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
                LOG_ERROR("CH347_WriteRead send usb data failure.");
                return;
            }
            BI = 0;
        }
    }

    // 清空while循环中剩余的命令
    if (BI > 0) {
        TxLen = BI;
        if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
            LOG_ERROR("CH347_WriteRead send usb data failure.");
            return;
        }
        BI = 0;
    }

    // 构建输出最后1位TDI数据的命令包
    if (bits) {
        BitBangPkt[BI++] = IsRead ? CH347_CMD_JTAG_BIT_OP_RD : CH347_CMD_JTAG_BIT_OP;
        DLen = (nb1 * 2) + 1;
        BitBangPkt[BI++] = (uint8_t)(DLen >> 0) & 0xFF;
        BitBangPkt[BI++] = (uint8_t)(DLen >> 8) & 0xFF; 
        TMS_Bit = TMS_L;

        for (i = 0; i < nb1; i++) {
            if ((bits[nb8] >> i) & 1)
                TDI_Bit = TDI_H;
            else
                TDI_Bit = TDI_L;

            if ((i + 1) == nb1) //最后一位在Exit1-DR状态输出
                TMS_Bit = TMS_H;
            BitBangPkt[BI++] = TMS_Bit | TDI_Bit | TCK_L;
            BitBangPkt[BI++] = TMS_Bit | TDI_Bit | TCK_H;
        }
        BitBangPkt[BI++] = TMS_Bit | TDI_Bit | TCK_L;
    }

    // 读取Bit-Bang模式下的最后一字节数据
    if (nb1 && IsRead) {
        TxLen = BI;

        if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
            LOG_ERROR("CH347_WriteRead send usb data failure.");
            return;
        }
        BI = 0;

        RxLen = TxLen + CH347_CMD_HEADER;
        if (!(ret = CH347_Read(temp, &RxLen))) {
            LOG_ERROR("CH347_WriteRead read usb data failure.");
            return;
        }

        for (i = 0; ret == true && i < nb1; i++) {
            if (temp[CH347_CMD_HEADER + i] & 1)
                tdos[nb8] |= (1 << i);
            else
                tdos[nb8] &= ~(1 << i);
        }
    }

    // 清空此次批量读写函数中未处理命令
    if (BI > 0) {
        TxLen = BI;
        if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
            LOG_ERROR("CH347_WriteRead send usb data failure.");
            return;
        }
        BI = 0;
    }

    if (bits) {
        memcpy(bits, tdos, DIV_ROUND_UP(nb_bits, 8));
    }

    free(tdos);
    LOG_DEBUG_IO("bits %d str value: [%s].\n", DIV_ROUND_UP(nb_bits, 8), HexToString(bits, DIV_ROUND_UP(nb_bits, 8)));

    // 将TCK、TDI拉低为低电平，因TDI采样在TCK上升沿，若状态未改变，则TDI采样将可能发生在TCK下降沿
    BI = CH347_CMD_HEADER;
    BI = CH347_IdleClock(BitBangPkt, BI);

    BitBangPkt[0] = CH347_CMD_JTAG_BIT_OP;
    BitBangPkt[1] = (unsigned char)BI - CH347_CMD_HEADER;
    BitBangPkt[2] = 0;

    TxLen = BI;

    if (!CH347_Write(BitBangPkt, &TxLen) && (TxLen != BI)) {
        LOG_ERROR("JTAG Write send usb data failure.");
        return;
    }
}

static void CH347_RunTest(int cycles, tap_state_t state)
{
    LOG_DEBUG_IO("%s(cycles=%i, end_state=%d)", __func__, cycles, state);
    CH347_MoveState(TAP_IDLE, 0);

    CH347_WriteRead(NULL, cycles, SCAN_OUT);
    CH347_MoveState(state, 0);
}

static void CH347_TableClocks(int cycles)
{
    LOG_DEBUG_IO("%s(cycles=%i)", __func__, cycles);
    CH347_WriteRead(NULL, cycles, SCAN_OUT);
}

/**
 * CH347_Scan - 切换至SHIFT-DR或者SHIFT-IR状态进行扫描
 * @param cmd 	    上层传递命令参数
 *
 * @return 	        成功返回ERROR_OK
 */
static int CH347_Scan(struct scan_command *cmd)
{
    int scan_bits;
    uint8_t *buf = NULL;
    enum scan_type type;
    int ret = ERROR_OK;
    static const char *const type2str[] = {"", "SCAN_IN", "SCAN_OUT", "SCAN_IO"};
    char *log_buf = NULL;

    type = jtag_scan_type(cmd);
    scan_bits = jtag_build_buffer(cmd, &buf);

    if (cmd->ir_scan)
        CH347_MoveState(TAP_IRSHIFT, 0);
    else
        CH347_MoveState(TAP_DRSHIFT, 0);

    log_buf = HexToString(buf, DIV_ROUND_UP(scan_bits, 8));
    LOG_DEBUG_IO("Scan");
    LOG_DEBUG_IO("%s(scan=%s, type=%s, bits=%d, buf=[%s], end_state=%d)", __func__,
                 cmd->ir_scan ? "IRSCAN" : "DRSCAN",
                 type2str[type],
                 scan_bits, log_buf, cmd->end_state);

    free(log_buf);

    CH347_WriteRead(buf, scan_bits, type);

    ret = jtag_read_buffer(buf, cmd);
    free(buf);

    CH347_MoveState(cmd->end_state, 1);

    return ret;
}

static void CH347_Sleep(int us)
{
    LOG_DEBUG_IO("%s(us=%d)", __func__, us);
    jtag_sleep(us);
}

static int ch347_execute_queue(void)
{
    struct jtag_command *cmd;
    static int first_call = 1;
    int ret = ERROR_OK;

    if (first_call) {
        first_call--;
        CH347_Reset();
    }

    for (cmd = jtag_command_queue; ret == ERROR_OK && cmd;
         cmd = cmd->next) {
        switch (cmd->type) {
        case JTAG_RESET:
            CH347_Reset();
            break;
        case JTAG_RUNTEST:
            CH347_RunTest(cmd->cmd.runtest->num_cycles,
                          cmd->cmd.runtest->end_state);
            break;
        case JTAG_STABLECLOCKS:
            CH347_TableClocks(cmd->cmd.stableclocks->num_cycles);
            break;
        case JTAG_TLR_RESET:
            CH347_MoveState(cmd->cmd.statemove->end_state, 0);
            break;
        case JTAG_PATHMOVE:
            CH347_MovePath(cmd->cmd.pathmove);
            break;
        case JTAG_TMS:
            CH347_TMS(cmd->cmd.tms);
            break;
        case JTAG_SLEEP:
            CH347_Sleep(cmd->cmd.sleep->us);
            break;
        case JTAG_SCAN:
            ret = CH347_Scan(cmd->cmd.scan);
            break;
        default:
            LOG_ERROR("BUG: unknown JTAG command type 0x%X",
                      cmd->type);
            ret = ERROR_FAIL;
            break;
        }
    }
    return ret;
}

/**
 * ch347_init - CH347 初始化函数
 *
 *  执行工作：
 *                初始化动态库函数
 *                打开设备
 *  @return 	  成功返回0,失败返回ERROR_FAIL
 */
static int ch347_init(void)
{
    unsigned char clearBuffer[4096] = "";
    unsigned long RxLen = 4096;
#ifdef _WIN32
    //printf("%s-%d-WIN32\n", __func__, __LINE__);
    if (uhModule == 0) {
        uhModule = LoadLibrary("CH347DLL.DLL");
        if (uhModule) {
            CH347OpenDevice = (pCH347OpenDevice)GetProcAddress(uhModule, "CH347OpenDevice");
            CH347CloseDevice = (pCH347CloseDevice)GetProcAddress(uhModule, "CH347CloseDevice");
            CH347ReadData = (pCH347ReadData)GetProcAddress(uhModule, "CH347ReadData");
            CH347WriteData = (pCH347WriteData)GetProcAddress(uhModule, "CH347WriteData");
            CH347SetTimeout = (pCH347SetTimeout)GetProcAddress(uhModule, "CH347SetTimeout");
            CH347Jtag_INIT = (pCH347Jtag_INIT)GetProcAddress(uhModule, "CH347Jtag_INIT");
            if (CH347OpenDevice == NULL || CH347CloseDevice == NULL || CH347SetTimeout == NULL || CH347ReadData == NULL || CH347WriteData == NULL || CH347Jtag_INIT == NULL) {
                LOG_ERROR("Jtag_init error ");
                return ERROR_FAIL;
            }
        }
    }
    DevIsOpened = CH347OpenDevice(ugIndex);
#elif defined(__linux__)
    //printf("%s-%d-LINUX\n", __func__, __LINE__);
    DevIsOpened = CH347OpenDevice(ugIndex);
    ugIndex = DevIsOpened;
#endif
    if (!DevIsOpened) {
        LOG_ERROR("CH347 Open Error.");
        return ERROR_FAIL;
    }

    USBC_PACKET = USBC_PACKET_USBHS; // 默认为USB2.0高速，其单次传输USB包大小512字节

    if (!CH347_Read(clearBuffer, &RxLen)) {
        LOG_ERROR("CH347 clear Buffer Error.");
        return ERROR_FAIL;
    }

    // CH347SetTimeout(ugIndex, 5000, 5000);

    tap_set_state(TAP_RESET);
    return 0;
}

/**
 * ch347_quit - CH347 设备释放函数
 *
 * 执行工作：
 *              复位JTAG引脚信号
 *              关闭
 *  @return 	一直返回0
 */
static int ch347_quit(void)
{
    // 退出前将信号线全部设置为低电平
    unsigned long retlen = 5;
    unsigned char byte[5] = {CH347_CMD_JTAG_BIT_OP, 0x01, 0x00, 0x00, 0x00};

    CH347_Write(byte, &retlen);

    if (DevIsOpened) {
        CH347CloseDevice(ugIndex);
        LOG_INFO("Close the CH347.");
        DevIsOpened = false;
    }
    return 0;
}

/**
 * ch347_speed - CH347 TCK频率设置
 *  @param speed 设置的频率大小
 *  @return 	 成功返回ERROR_OK，失败返回FALSE
 */
static int ch347_speed(int speed)
{
    unsigned long i = 0;
    int retval = -1;
    int speed_clock[6] = {MHZ(1.875), MHZ(3.75), MHZ(7.5), MHZ(15), MHZ(30), MHZ(60)};

    for (i = 0; i < (sizeof(speed_clock) / sizeof(int)); i++) {
        if ((speed >= speed_clock[i]) && (speed <= speed_clock[i + 1])) {
            retval = CH347Jtag_INIT(ugIndex, i + 1);
            if (!retval) {
                LOG_ERROR("Couldn't set CH347 TCK speed");
                return retval;
            } else {
                break;
            }
        } else if (speed < speed_clock[0]) {
            retval = CH347Jtag_INIT(ugIndex, 0);
            if (!retval) {
                LOG_ERROR("Couldn't set CH347 TCK speed");
                return retval;
            } else {
                break;
            }
        }
    }

    return ERROR_OK;
}

static int ch347_speed_div(int speed, int *khz)
{
    *khz = speed / 1000;
    return ERROR_OK;
}

static int ch347_khz(int khz, int *jtag_speed)
{
    if (khz == 0) {
        LOG_ERROR("Couldn't support the adapter speed");
        return ERROR_FAIL;
    }
    *jtag_speed = khz * 1000;
    return ERROR_OK;
}

COMMAND_HANDLER(ch347_handle_vid_pid_command)
{
    // TODO
    return ERROR_OK;
}

static const struct command_registration ch347_subcommand_handlers[] = {
    {
        .name = "vid_pid",
        .handler = ch347_handle_vid_pid_command,
        .mode = COMMAND_CONFIG,
        .help = "",
        .usage = "",
    },
    COMMAND_REGISTRATION_DONE};

static const struct command_registration ch347_command_handlers[] = {
    {
        .name = "ch347",
        .mode = COMMAND_ANY,
        .help = "perform ch347 management",
        .chain = ch347_subcommand_handlers,
        .usage = "",
    },
    COMMAND_REGISTRATION_DONE};

static struct jtag_interface ch347_interface = {
    .supported = DEBUG_CAP_TMS_SEQ,
    .execute_queue = ch347_execute_queue,
};

struct adapter_driver ch347_adapter_driver = {
    .name = "ch347",
    .transports = jtag_only,
    .commands = ch347_command_handlers,

    .init = ch347_init,
    .quit = ch347_quit,
    .speed = ch347_speed,
    .khz = ch347_khz,
    .speed_div = ch347_speed_div,

    .jtag_ops = &ch347_interface,
};
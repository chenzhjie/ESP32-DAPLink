/**
 * @file spi_op.c
 * @author windowsair
 * @brief Using SPI for common transfer operations
 * @change: 2021-3-7 Support esp32 SPI
 *          2021-3-10 Support 3-wire spi
 * @version 0.2
 * @date 2021-3-10
 *
 * @copyright Copyright (c) 2021
 *
 */
#include <stdio.h>


#include "cmsis_compiler.h"

#include "spi_op.h"
#include "spi_switch.h"

// soc register
#include "esp32/rom/gpio.h"
#include "esp32s3/include/soc/gpio_struct.h"
#include "hal/gpio_types.h"

#include "esp32s3/include/soc/dport_access.h"
#include "esp32s3/include/soc/dport_reg.h"
#include "esp32s3/include/soc/periph_defs.h"
#include "esp32s3/include/soc/spi_struct.h"
#include "esp32s3/include/soc/spi_reg.h"

#define DAP_SPI GPSPI2

/**
 * @brief Calculate integer division and round up
 *
 * @param A
 * @param B
 * @return result
 */
__STATIC_FORCEINLINE int div_round_up(int A, int B)
{
    return (A + B - 1) / B;
}


/**
 * @brief Write bits. LSB & little-endian
 *        Note: No check. The pointer must be valid.
 * @param count Number of bits to be written (<= 64 bits, no length check)
 * @param buf Data Buf
 */
void DAP_SPI_WriteBits(const uint8_t count, const uint8_t *buf)
{
    DAP_SPI.user.usr_command = 0;
    DAP_SPI.user.usr_addr = 0;

    // have data to send
    DAP_SPI.user.usr_mosi = 1;
    DAP_SPI.ms_dlen.ms_data_bitlen = count - 1;
    // copy data to reg
    switch (count)
    {
    case 8:
        DAP_SPI.data_buf[0] = (buf[0] << 0) | (0U << 8) | (0U << 16) | (0U << 24);
        break;
    case 16:
        DAP_SPI.data_buf[0] = (buf[0] << 0) | (buf[1] << 8) | (0x000U << 16) | (0x000U << 24);
        break;
    case 33: // 32bits data & 1 bit parity
        DAP_SPI.data_buf[0] = (buf[0] << 0) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        DAP_SPI.data_buf[1] = (buf[4] << 0) | (0x000U << 8) | (0x000U << 16) | (0x000U << 24);
        break;
    case 51: // for line reset
        DAP_SPI.data_buf[0] = (buf[0] << 0) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        DAP_SPI.data_buf[1] = (buf[4] << 0) | (buf[5] << 8) | (buf[2] << 16) | (0x000U << 24);
        break;
    default:
    {
        uint32_t data_buf[2];
        uint8_t *pData = (uint8_t *)data_buf;
        int i;

        for (i = 0; i < div_round_up(count, 8); i++)
        {
            pData[i] = buf[i];
        }
        // last byte use mask:
        pData[i-1] = pData[i-1] & ((2U >> (count % 8)) - 1U);

        DAP_SPI.data_buf[0] = data_buf[0];
        DAP_SPI.data_buf[1] = data_buf[1];
    }
    }

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for sending to complete
    while (DAP_SPI.cmd.usr) continue;
}



/**
 * @brief Read bits. LSB & little-endian
 *        Note: No check. The pointer must be valid.
 * @param count Number of bits to be read (<= 64 bits, no length check)
 * @param buf Data Buf
 */
void DAP_SPI_ReadBits(const uint8_t count, uint8_t *buf) {
    int i;
    uint32_t data_buf[2];

    uint8_t * pData = (uint8_t *)data_buf;

    DAP_SPI.user.usr_mosi = 0;
    DAP_SPI.user.usr_miso = 1;

    DAP_SPI.user.sio = true;
    DAP_SPI.ms_dlen.ms_data_bitlen = count - 1U;

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for reading to complete
    while (DAP_SPI.cmd.usr) continue;

    DAP_SPI.user.sio = false;

    data_buf[0] = DAP_SPI.data_buf[0];
    data_buf[1] = DAP_SPI.data_buf[1];

    for (i = 0; i < div_round_up(count, 8); i++)
    {
        buf[i] = pData[i];
    }
    // last byte use mask:
    buf[i-1] = buf[i-1] & ((2 >> (count % 8)) - 1);
}


/**
 * @brief Step1: Packet Request
 *
 * @param packetHeaderData data from host
 * @param ack ack from target
 * @param TrnAfterACK num of trn after ack
 */
__FORCEINLINE void DAP_SPI_Send_Header(const uint8_t packetHeaderData, uint8_t *ack, uint8_t TrnAfterACK)
{
    volatile uint32_t dataBuf;

    // have data to send
    DAP_SPI.user.usr_mosi = 1;
    DAP_SPI.ms_dlen.ms_data_bitlen = 8 - 1;

    DAP_SPI.user.usr_miso = 1;

    DAP_SPI.user.sio = true;

    // 1 bit Trn(Before ACK) + 3bits ACK + TrnAferACK  - 1(prescribed)
    DAP_SPI.ms_dlen.ms_data_bitlen = 1U + 3U + TrnAfterACK - 1U;

    // copy data to reg
    DAP_SPI.data_buf[0] = (packetHeaderData << 0) | (0U << 8) | (0U << 16) | (0U << 24);

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for sending to complete
    while (DAP_SPI.cmd.usr) continue;

    DAP_SPI.user.sio = false;

    dataBuf = DAP_SPI.data_buf[0];
    *ack = (dataBuf >> 1) & 0b111;
}


/**
 * @brief Step2: Read Data
 *
 * @param resData data from target
 * @param resParity parity from target
 */
__FORCEINLINE void DAP_SPI_Read_Data(uint32_t *resData, uint8_t *resParity)
{
    volatile uint64_t dataBuf;
    uint32_t *pU32Data = (uint32_t *)&dataBuf;

    DAP_SPI.user.usr_mosi = 0;
    DAP_SPI.user.usr_miso = 1;

    DAP_SPI.user.sio = true;

    // 1 bit Trn(End) + 3bits ACK + 32bis data + 1bit parity - 1(prescribed)
    DAP_SPI.ms_dlen.ms_data_bitlen = 1U + 32U + 1U - 1U;

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for sending to complete
    while (DAP_SPI.cmd.usr) continue;

    pU32Data[0] = DAP_SPI.data_buf[0];
    pU32Data[1] = DAP_SPI.data_buf[1];

    DAP_SPI.user.sio = false;

    *resData = (dataBuf >> 0U) & 0xFFFFFFFFU;  // 32bits Response Data
    *resParity = (dataBuf >> (0U + 32U)) & 1U; // 3bits ACK + 32bis data
}

/**
 * @brief Step2: Write Data
 *
 * @param data data from host
 * @param parity parity from host
 */
__FORCEINLINE void DAP_SPI_Write_Data(uint32_t data, uint8_t parity)
{
    DAP_SPI.user.usr_mosi = 1;
    DAP_SPI.user.usr_miso = 0;

    DAP_SPI.ms_dlen.ms_data_bitlen = 32U + 1U - 1U; // 32bis data + 1bit parity - 1(prescribed)

    // copy data to reg
    DAP_SPI.data_buf[0] = data;
    DAP_SPI.data_buf[1] = parity;

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for sending to complete
    while (DAP_SPI.cmd.usr) continue;
}

/**
 * @brief Generate Clock Cycle
 *
 * @param num Cycle Num
 */
__FORCEINLINE void DAP_SPI_Generate_Cycle(uint8_t num)
{
    //// TODO: It may take long time to generate just one clock
    DAP_SPI.user.usr_mosi = 1;
    DAP_SPI.user.usr_miso = 0;
    DAP_SPI.ms_dlen.ms_data_bitlen = num - 1U;

    DAP_SPI.data_buf[0] = 0x00000000U;

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for sending to complete
    // while (DAP_SPI.cmd.usr) continue;
    // 200us reduce
}

/**
 * @brief Quickly generate 1 clock
 *
 */
__FORCEINLINE void DAP_SPI_Fast_Cycle()
{
    DAP_SPI_Release();
    DAP_SPI_Acquire();
}


/**
 * @brief Generate Protocol Error Cycle
 *
 */
__FORCEINLINE void DAP_SPI_Protocol_Error_Read()
{
    DAP_SPI.user.usr_mosi = 1;
    DAP_SPI.user.usr_miso = 0;
    DAP_SPI.ms_dlen.ms_data_bitlen = 32U + 1U - 1; // 32bit ignore data + 1 bit - 1(prescribed)

    DAP_SPI.data_buf[0] = 0xFFFFFFFFU;
    DAP_SPI.data_buf[1] = 0xFFFFFFFFU;

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for sending to complete
    while (DAP_SPI.cmd.usr) continue;
}


/**
 * @brief Generate Protocol Error Cycle
 *
 */
__FORCEINLINE void DAP_SPI_Protocol_Error_Write()
{
    DAP_SPI.user.usr_mosi = 1;
    DAP_SPI.user.usr_miso = 0;
    DAP_SPI.ms_dlen.ms_data_bitlen = 1U + 32U + 1U - 1; // 1bit Trn + 32bit ignore data + 1 bit - 1(prescribed)

    DAP_SPI.data_buf[0] = 0xFFFFFFFFU;
    DAP_SPI.data_buf[1] = 0xFFFFFFFFU;

    // Start transmission
    DAP_SPI.cmd.usr = 1;
    // Wait for sending to complete
    while (DAP_SPI.cmd.usr) continue;
}
